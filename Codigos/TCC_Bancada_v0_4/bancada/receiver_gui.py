"""Aplicativo da estação: somente recebe. Não precisa do USB da bancada."""
from pathlib import Path
import queue
import time
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from .serial_link import SerialLink, ports
from .radio_file import parse_frame, FileReceiver
from .live_storage import load_flash

class ReceiverApp:
    def __init__(self, root):
        self.root = root
        root.title('Estação LoRa | Registros da bancada — v0.3.0')
        root.geometry('1060x750'); root.minsize(900, 650)
        self.folder = Path.home() / 'TCC_Bancada' / 'recebidos_lora'
        self.events = queue.Queue(maxsize=6000)
        self.link = None
        self.receiver = None
        self.receiver_key = None
        self.last_packet = 0
        self.invalid = self.other = 0
        self.shown_path = None
        self.port = tk.StringVar()
        self.status = tk.StringVar(value='Selecione o USB do receptor BRAVO e conecte.')
        self.detail = tk.StringVar(value='Este notebook só recebe. Os comandos do ensaio continuam no computador da bancada.')
        self.progress_text = tk.StringVar(value='Nenhum arquivo recebido.')
        self.path_text = tk.StringVar(value=f'Destino: {self.folder}')
        self.link_text = tk.StringVar(value='Sem pacotes de arquivo.')
        style = ttk.Style(root); style.theme_use('clam')
        style.configure('TButton', padding=(8, 6))
        header = tk.Frame(root, bg='#152b46', padx=18, pady=15); header.pack(fill='x')
        tk.Label(header, text='ESTAÇÃO LoRa • REGISTRO DO ENSAIO', bg='#152b46', fg='white', font=('Segoe UI', 17, 'bold')).pack(anchor='w')
        tk.Label(header, text='Recepção do arquivo completo • v0.3.0 • dados brutos, sem calibração', bg='#152b46', fg='#bdcfe2').pack(anchor='w', pady=(5, 0))
        top = ttk.Frame(root, padding=12); top.pack(fill='x')
        self.port_box = ttk.Combobox(top, textvariable=self.port, state='readonly', width=37)
        self.port_box.pack(side='left', padx=(0, 8))
        self.scan_button = ttk.Button(top, text='Atualizar', command=self.refresh_ports); self.scan_button.pack(side='left', padx=3)
        self.connect_button = ttk.Button(top, text='Conectar receptor', command=self.connect); self.connect_button.pack(side='left', padx=3)
        self.disconnect_button = ttk.Button(top, text='Desconectar', command=self.disconnect); self.disconnect_button.pack(side='left', padx=3)
        self.folder_button = ttk.Button(top, text='Pasta de destino…', command=self.choose_folder); self.folder_button.pack(side='left', padx=3)
        body = ttk.Frame(root, padding=(14, 4, 14, 10)); body.pack(fill='both', expand=True)
        ttk.Label(body, textvariable=self.status, font=('Segoe UI', 12, 'bold'), wraplength=1000).pack(anchor='w')
        self.progress = ttk.Progressbar(body, maximum=100); self.progress.pack(fill='x', pady=9)
        ttk.Label(body, textvariable=self.progress_text).pack(anchor='w')
        self.fig = Figure(figsize=(9, 4), dpi=100, layout='constrained')
        self.ax = self.fig.add_subplot(111)
        self.ax.set_xlabel('Tempo desde o início da aquisição (s)')
        self.ax.set_ylabel('Leitura bruta (contagens)')
        self.ax.set_title('O gráfico será exibido após verificar o registro recebido', loc='left', fontsize=11)
        self.ax.grid(True, alpha=.2)
        self.line, = self.ax.plot([], [], color='#1769aa', linewidth=1)
        self.canvas = FigureCanvasTkAgg(self.fig, master=body)
        self.canvas.get_tk_widget().pack(fill='both', expand=True, pady=(10, 0))
        NavigationToolbar2Tk(self.canvas, body, pack_toolbar=False).pack(fill='x')
        ttk.Label(body, textvariable=self.detail, wraplength=1000).pack(anchor='w', pady=(8, 2))
        ttk.Label(body, textvariable=self.link_text).pack(anchor='w', pady=2)
        ttk.Label(body, textvariable=self.path_text, wraplength=1000).pack(anchor='w', pady=2)
        self.refresh_ports(); self.set_controls()
        root.protocol('WM_DELETE_WINDOW', self.close)
        root.after(100, self.tick)

    def refresh_ports(self):
        if self.link: return
        self.port_map = {f'{p} — {description}': p for p, description in ports()}
        names = list(self.port_map)
        self.port_box.configure(values=names)
        if self.port.get() not in names: self.port.set(names[0] if names else '')

    def set_controls(self):
        connected = self.link is not None
        self.port_box.configure(state='disabled' if connected else 'readonly')
        for widget in (self.scan_button, self.connect_button, self.folder_button):
            widget.configure(state='disabled' if connected else 'normal')
        self.disconnect_button.configure(state='normal' if connected else 'disabled')

    def connect(self):
        try:
            port = self.port_map.get(self.port.get())
            if not port: raise ValueError('Selecione a porta USB da estação receptora.')
            self.link = SerialLink(port, 'LORA', self.events)
            self.status.set('CONECTADO • aguardando envio do registro')
            self.detail.set('No computador da bancada, abra Arquivos no ESP32, selecione o ensaio e clique Enviar por LoRa.')
        except Exception as exc:
            self.link = None
            messagebox.showerror('Não foi possível conectar', f'{exc}\nFeche o Monitor Serial e outros programas que usam essa porta.', parent=self.root)
        self.set_controls()

    def disconnect(self):
        if self.link: self.link.close(); self.link = None
        if self.receiver: self.receiver.close(); self.receiver = None; self.receiver_key = None
        while True:
            try: self.events.get_nowait()
            except queue.Empty: break
        self.status.set('DESCONECTADO • cópias e blocos já gravados foram preservados')
        self.set_controls()

    def choose_folder(self):
        path = filedialog.askdirectory(title='Pasta dos registros recebidos por LoRa', parent=self.root)
        if path:
            self.folder = Path(path)
            self.path_text.set(f'Destino: {self.folder}')

    def receive_line(self, line, stamp):
        frame = parse_frame(line)
        if frame is None:
            self.other += 1
            if "TCC2;" in line:
                self.detail.set('Recebendo telemetria ao vivo. Para o arquivo completo, encerre o ensaio e use Enviar por LoRa.')
            return
        if frame.key != self.receiver_key:
            if self.receiver: self.receiver.close()
            self.receiver = None; self.receiver_key = None
            receiver = FileReceiver(self.folder, frame)
            self.receiver = receiver; self.receiver_key = frame.key
            self.shown_path = None
            self.line.set_data([], []); self.canvas.draw_idle()
        self.receiver.accept(frame)
        self.last_packet = stamp
        r = self.receiver
        self.progress['value'] = 100 * len(r.received) / r.blocks
        self.progress_text.set(f'{len(r.received)} / {r.blocks} blocos · Faltam {r.missing} · Repetidos {r.duplicates} · Inválidos {self.invalid}')
        self.path_text.set(f'Destino: {r.folder}')
        if r.validation_error:
            self.status.set('ARQUIVO COM INCONSISTÊNCIA • não aprovado na verificação')
            self.detail.set(r.validation_error + ' A cópia foi preservada. Confira o registro original na bancada antes de repetir.')
        elif r.complete:
            self.status.set('RECEBIDO E VERIFICADO • arquivo completo')
            self.detail.set('Tamanho, CRC32 do arquivo, CRC das amostras e sequência conferidos. LOG e CSV foram salvos.')
            if r.record_status and not r.record_status['stop_confirmed']:
                self.status.set('ARQUIVO RECEBIDO • aquisição original sem encerramento confirmado')
                self.detail.set('A transferência está íntegra, mas o registro do ESP32 não contém confirmação de fim do ensaio.')
            elif r.record_status and r.record_status['reason'] not in ('MANUAL', 'LIMIT'):
                self.detail.set('Arquivo transferido e verificado. A aquisição original terminou com a ocorrência: ' + str(r.record_status['reason']))
            if self.shown_path != r.path:
                self.show_record(r.path); self.shown_path = r.path
        elif r.ended_pass:
            self.status.set(f'RECEPÇÃO INCOMPLETA • faltam {r.missing} blocos')
            self.detail.set('Envie novamente o mesmo registro na bancada. Os blocos recebidos serão aproveitados. Primeiras faltas: ' + ', '.join(map(str, r.missing_indices())))
        else:
            self.status.set('RECEBENDO ARQUIVO PELO LoRa')
            self.detail.set('Aguarde a verificação final. O fim do envio no transmissor não confirma a recepção deste arquivo.')

    def show_record(self, path):
        meta, acquisition = load_flash(path)
        samples = acquisition.traces['USB'].samples
        step = max(1, len(samples) // 5000)
        drawn = samples[::step]
        if samples and (not drawn or drawn[-1] != samples[-1]): drawn.append(samples[-1])
        self.line.set_data([p.t_us / 1e6 for p in drawn], [p.raw if p.flag == 'OK' else float('nan') for p in drawn])
        self.ax.relim(); self.ax.autoscale_view()
        self.ax.set_title(f'Registro recebido via LoRa • {len(samples)} amostras • SEM CALIBRAÇÃO', loc='left', fontsize=11)
        self.canvas.draw_idle()

    def tick(self):
        try:
            for _ in range(300):
                try: source, kind, value, stamp = self.events.get_nowait()
                except queue.Empty: break
                if kind == 'line':
                    try: self.receive_line(value, stamp)
                    except (ValueError, UnicodeError, KeyError, TypeError) as exc:
                        self.invalid += 1
                        self.detail.set(f'Dado ou arquivo rejeitado: {exc}')
                        self.status.set('CONFERÊNCIA PENDENTE • não considerar recebido integralmente')
                elif kind == 'error':
                    self.disconnect()
                    self.detail.set(f'Conexão perdida: {value}. Reconecte e reenvie o mesmo registro para completar as partes faltantes.')
                else: self.invalid += 1
            if self.last_packet:
                elapsed = time.monotonic() - self.last_packet
                rssi = self.receiver.last_rssi if self.receiver else None
                self.link_text.set(f'Último quadro de arquivo há {elapsed:.1f} s · RSSI: {rssi if rssi is not None else "—"} dBm')
                if self.receiver and not self.receiver.complete and not self.receiver.validation_error and elapsed > 8:
                    self.status.set(f'AGUARDANDO DADOS • ainda faltam {self.receiver.missing} blocos')
            if self.link and self.link.dropped:
                self.detail.set(f'Fila excedida: {self.link.dropped} mensagens. Reenvie o registro para preencher as faltas.')
        except OSError as exc:
            try: self.disconnect()
            except OSError: pass
            self.status.set('FALHA AO GRAVAR • recepção interrompida')
            self.detail.set(str(exc))
        self.root.after(100, self.tick)

    def close(self):
        try: self.disconnect()
        except OSError as exc:
            messagebox.showerror('Falha ao salvar', str(exc), parent=self.root)
        self.root.destroy()

def main():
    root = tk.Tk()
    ReceiverApp(root)
    root.mainloop()
