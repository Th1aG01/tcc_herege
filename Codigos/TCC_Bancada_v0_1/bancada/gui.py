"""Interface Tkinter. A fonte de amostras e a persistência ficam em módulos próprios."""
from pathlib import Path
from time import perf_counter
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from .core import Calibration, SimulationConfig, KGF_TO_N
from .simulator import Simulator
from .storage import SessionWriter, load_session

class App:
    def __init__(self, root):
        self.root = root
        root.title('Bancada de empuxo | TG2 — v0.1.0')
        root.geometry('1120x740')
        root.minsize(940, 660)
        root.configure(bg='#edf2f7')
        self.running = False
        self.writer = None
        self.samples = []
        self.calibration = Calibration()
        self.after_id = None
        self.folder = Path.home() / 'TCC_Bancada' / 'ensaios'
        self.name = tk.StringVar(value='Ensaio de demonstração')
        self.duration = tk.StringVar(value='12')
        self.peak = tk.StringVar(value='1000')
        self.rate = tk.StringVar(value='80')
        self.status = tk.StringVar(value='PRONTO • aguardando início')
        self.detail = tk.StringVar(value='Os dados serão salvos automaticamente durante a aquisição simulada.')
        self.path_text = tk.StringVar(value=f'Pasta de dados: {self.folder}')
        style = ttk.Style(root)
        style.theme_use('clam')
        style.configure('TFrame', background='#edf2f7')
        style.configure('TLabel', background='#edf2f7', foreground='#152b46', font=('Segoe UI', 10))
        style.configure('TButton', font=('Segoe UI', 10), padding=(10, 8))
        style.configure('Accent.TButton', background='#155eaa', foreground='white')
        style.map('Accent.TButton', background=[('disabled', '#abb8c5'), ('active', '#104e8e')])
        header = tk.Frame(root, bg='#152b46', padx=24, pady=16)
        header.pack(fill='x')
        tk.Label(header, text='BANCADA DE EMPUXO', bg='#152b46', fg='white',
                 font=('Segoe UI', 19, 'bold')).pack(anchor='w')
        tk.Label(header, text='TG2  /  Aquisição e registro  /  v0.1.0', bg='#152b46',
                 fg='#bdcfe2', font=('Segoe UI', 10)).pack(anchor='w', pady=(4, 0))
        tk.Label(root, text='MODO SIMULAÇÃO  •  Dados artificiais  •  Nenhuma conexão com o hardware',
                 bg='#fff0ce', fg='#654600', pady=10, font=('Segoe UI', 11, 'bold')).pack(fill='x')
        body = ttk.Frame(root, padding=18)
        body.pack(fill='both', expand=True)
        body.columnconfigure(1, weight=1)
        body.rowconfigure(0, weight=1)
        controls = ttk.Frame(body, padding=(0, 0, 18, 0))
        controls.grid(row=0, column=0, sticky='nsew')
        ttk.Label(controls, text='CONFIGURAR ENSAIO', font=('Segoe UI', 11, 'bold')).pack(anchor='w', pady=(0, 14))
        self.inputs = []
        for label, var in [('Nome do ensaio', self.name), ('Duração da simulação (s)', self.duration),
                           ('Amplitude de referência (N)', self.peak)]:
            ttk.Label(controls, text=label).pack(anchor='w', pady=(6, 3))
            entry = ttk.Entry(controls, textvariable=var, width=28)
            entry.pack(fill='x')
            self.inputs.append(entry)
        ttk.Label(controls, text='Taxa simulada (amostras/s)').pack(anchor='w', pady=(10, 3))
        self.rate_box = ttk.Combobox(controls, textvariable=self.rate, values=('10', '80'), state='readonly', width=26)
        self.rate_box.pack(fill='x')
        ttk.Label(controls, text='Observações').pack(anchor='w', pady=(10, 3))
        self.notes = tk.Text(controls, width=28, height=4, font=('Segoe UI', 10), wrap='word', relief='solid', bd=1)
        self.notes.pack(fill='x')
        self.start_button = ttk.Button(controls, text='▶  Iniciar simulação', command=self.start, style='Accent.TButton')
        self.start_button.pack(fill='x', pady=(16, 6))
        self.stop_button = ttk.Button(controls, text='■  Encerrar aquisição', command=self.stop, state='disabled')
        self.stop_button.pack(fill='x', pady=(0, 6))
        self.open_button = ttk.Button(controls, text='Abrir ensaio salvo…', command=self.open_session)
        self.open_button.pack(fill='x', pady=3)
        self.folder_button = ttk.Button(controls, text='Escolher pasta de dados…', command=self.choose_folder)
        self.folder_button.pack(fill='x', pady=3)
        ttk.Label(controls, text='CSV e JSON são salvos juntos.\nA calibração desta versão é fictícia.',
                  font=('Segoe UI', 9)).pack(anchor='w', pady=(12, 0))
        right = ttk.Frame(body)
        right.grid(row=0, column=1, sticky='nsew')
        cards = ttk.Frame(right)
        cards.pack(fill='x', pady=(0, 12))
        self.values = []
        for i, label in enumerate(('FORÇA (N)', 'FORÇA (kgf)', 'TEMPO (s)', 'AMOSTRAS')):
            cards.columnconfigure(i, weight=1)
            card = tk.Frame(cards, bg='white', padx=12, pady=10)
            card.grid(row=0, column=i, sticky='ew', padx=(0, 6))
            tk.Label(card, text=label, bg='white', fg='#53657a', font=('Segoe UI', 9)).pack(anchor='w')
            var = tk.StringVar(value='—')
            tk.Label(card, textvariable=var, bg='white', fg='#152b46', font=('Segoe UI', 19, 'bold')).pack(anchor='w')
            self.values.append(var)
        self.fig = Figure(figsize=(7, 4), dpi=100, layout='constrained')
        self.ax = self.fig.add_subplot(111)
        self.ax.set_xlabel('Tempo de aquisição (s)')
        self.ax.set_ylabel('Força (N)')
        self.ax.set_title('Curva de força • SIMULADA', loc='left', fontsize=12)
        self.ax.grid(True, alpha=0.18)
        self.line, = self.ax.plot([], [], color='#1769aa', linewidth=1.7)
        self.ax.set_xlim(0, 12)
        self.ax.set_ylim(-50, 1150)
        self.canvas = FigureCanvasTkAgg(self.fig, master=right)
        self.canvas.get_tk_widget().pack(fill='both', expand=True)
        toolbar = NavigationToolbar2Tk(self.canvas, right, pack_toolbar=False)
        toolbar.pack(fill='x')
        self.summary = tk.StringVar(value='Nenhum ensaio iniciado.')
        ttk.Label(right, textvariable=self.summary).pack(anchor='w', pady=(8, 0))
        footer = ttk.Frame(root, padding=(20, 8, 20, 14))
        footer.pack(fill='x')
        ttk.Label(footer, textvariable=self.status, font=('Segoe UI', 11, 'bold')).pack(anchor='w')
        ttk.Label(footer, textvariable=self.detail, wraplength=1020).pack(anchor='w', pady=3)
        ttk.Label(footer, textvariable=self.path_text, wraplength=1020, font=('Segoe UI', 9)).pack(anchor='w')
        root.protocol('WM_DELETE_WINDOW', self.close)
        self.canvas.draw()

    def set_running(self, running):
        self.running = running
        for widget in [self.start_button, self.open_button, self.folder_button, *self.inputs]:
            widget.configure(state='disabled' if running else 'normal')
        self.rate_box.configure(state='disabled' if running else 'readonly')
        self.notes.configure(state='disabled' if running else 'normal')
        self.stop_button.configure(state='normal' if running else 'disabled')

    def start(self):
        if self.running:
            return
        try:
            config = SimulationConfig(float(self.duration.get().replace(',', '.')), int(self.rate.get()),
                                      float(self.peak.get().replace(',', '.')))
            config.validate()
            calibration = Calibration()
            writer = SessionWriter(self.folder, self.name.get(), self.notes.get('1.0', 'end-1c'), config, calibration)
        except (ValueError, OSError) as exc:
            messagebox.showerror('Não foi possível iniciar', str(exc), parent=self.root)
            return
        self.writer = writer
        self.calibration = calibration
        self.source = Simulator(config, calibration)
        self.samples = []
        self.set_running(True)
        self.status.set('GRAVANDO • simulação em andamento')
        self.detail.set('Registro automático ativo no computador. Dados de demonstração.')
        self.path_text.set(f'Arquivo: {writer.csv_path}')
        self.started = perf_counter()
        self.ax.set_xlim(0, config.duration_s)
        self.ax.set_ylim(-config.peak_n*0.06, config.peak_n*1.15)
        self.tick()

    def collect(self):
        batch = self.source.read_until(perf_counter() - self.started)
        if batch:
            self.writer.append(batch)
            self.samples.extend(batch)

    def tick(self):
        self.after_id = None
        if not self.running:
            return
        try:
            self.collect()
            self.refresh()
            if self.source.finished:
                self.finish('completed')
            else:
                self.after_id = self.root.after(100, self.tick)
        except Exception as exc:
            self.fail(exc)

    def refresh(self):
        if not self.samples:
            for var in self.values: var.set('—')
            self.line.set_data([], [])
            self.summary.set('Arquivo sem amostras.')
        else:
            times = [s.t_us / 1e6 for s in self.samples]
            forces = [self.calibration.force(s.raw_counts) for s in self.samples]
            for var, value in zip(self.values, (f'{forces[-1]:.2f}', f'{forces[-1]/KGF_TO_N:.2f}',
                                               f'{times[-1]:.2f}', str(len(times)))):
                var.set(value)
            self.line.set_data(times, forces)
            self.summary.set(f'Pico observado: {max(forces):.2f} N  •  Dados brutos preservados  •  Sem filtro')
        self.canvas.draw_idle()

    def finish(self, status):
        if self.after_id is not None:
            self.root.after_cancel(self.after_id)
            self.after_id = None
        try:
            self.writer.finish(status)
        finally:
            self.set_running(False)
        self.status.set('CONCLUÍDO • ensaio salvo' if status == 'completed' else 'ENCERRADO PELO USUÁRIO • ensaio salvo')
        self.detail.set('Abra o CSV e seu JSON correspondente para revisar este ensaio. Origem: SIMULAÇÃO.')

    def stop(self):
        if not self.running:
            return
        try:
            self.collect()
            self.refresh()
            self.finish('completed' if self.source.finished else 'stopped')
        except Exception as exc:
            self.fail(exc)

    def fail(self, exc):
        if self.after_id is not None:
            self.root.after_cancel(self.after_id)
            self.after_id = None
        try:
            if self.writer and not self.writer.closed:
                self.writer.finish('error')
        except Exception:
            pass
        self.set_running(False)
        self.status.set('ERRO • aquisição interrompida')
        self.detail.set('O salvamento completo não está confirmado. Preserve os arquivos existentes.')
        messagebox.showerror('Falha na aquisição ou gravação', str(exc), parent=self.root)

    def choose_folder(self):
        path = filedialog.askdirectory(title='Escolher pasta para os próximos ensaios', parent=self.root)
        if path:
            self.folder = Path(path)
            self.path_text.set(f'Pasta dos próximos ensaios: {self.folder}')

    def open_session(self):
        path = filedialog.askopenfilename(title='Abrir CSV de ensaio simulado', initialdir=str(self.folder),
                                          filetypes=[('Amostras CSV', '*.csv')], parent=self.root)
        if not path:
            return
        self.open_path(Path(path))

    def open_path(self, path):
        if self.running:
            return
        try:
            meta, calibration, samples = load_session(path)
        except (ValueError, OSError, KeyError, TypeError) as exc:
            messagebox.showerror('Não foi possível abrir', f'{exc}\n\nMantenha o CSV e o JSON juntos, com o mesmo nome.', parent=self.root)
            return
        self.calibration, self.samples = calibration, samples
        self.name.set(meta.get('test_name', 'Ensaio salvo'))
        self.notes.delete('1.0', 'end')
        self.notes.insert('1.0', meta.get('notes', ''))
        times = [s.t_us/1e6 for s in samples]
        forces = [calibration.force(s.raw_counts) for s in samples]
        self.ax.set_xlim(0, max(times[-1] if times else 1, 1))
        low, high = (min(forces), max(forces)) if forces else (0, 1)
        margin = max((high-low)*0.1, 1)
        self.ax.set_ylim(min(0, low)-margin, high+margin)
        states = {'completed': 'concluído', 'stopped': 'encerrado pelo usuário',
                  'recording': 'registro sem encerramento confirmado', 'error': 'encerrado com erro'}
        self.status.set('ARQUIVO ABERTO • ' + states[meta['status']])
        self.detail.set('Origem: SIMULAÇÃO. Sequência, tempos e conversões conferidos na leitura.')
        self.path_text.set(f'Arquivo: {path}')
        self.refresh()
        if meta['status'] in ('recording', 'error'):
            messagebox.showwarning('Registro possivelmente incompleto',
                'Os dados existentes foram abertos, mas este ensaio não tem encerramento normal confirmado.', parent=self.root)

    def close(self):
        if self.running:
            if not messagebox.askyesno('Encerrar aquisição?', 'Encerrar o ensaio em andamento e fechar?', parent=self.root):
                return
            self.stop()
        self.root.destroy()

def main():
    root = tk.Tk()
    App(root)
    root.mainloop()
