"""Integração de bancada. Nunca envia comando de ignição."""
from collections import deque
from pathlib import Path
from time import monotonic
from uuid import uuid4
import os
import queue
import statistics
import subprocess
import sys
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from .protocol import Acquisition, parse_line
from .live_storage import LiveWriter, load_live, load_flash
from .serial_link import SerialLink, ports
from .timing import AcquisitionTiming

ERRORS = {
    'HX711_TIMEOUT': 'O HX711 ficou mais de 2 segundos sem disponibilizar nova leitura. Confira alimentação, GND, DT 33 e CLK 32.',
    'ESP32_REINICIADO': 'O ESP32 reiniciou durante a aquisição. Os dados já recebidos foram preservados.',
    'USB_DESCONECTADO': 'A comunicação USB foi perdida. Não foi possível confirmar o instante final no ESP32.',
    'SEM_CONFIRMACAO_USB': 'O ESP32 não confirmou o encerramento pelo USB.',
    'HX711_NO_DATA': 'O HX711 não forneceu leitura recente. Verifique alimentação, DT 33 e CLK 32.',
    'HX711_SATURATED': 'HX711 no limite do ADC. Verifique a célula e as conexões.',
    'FLASH_NOT_READY': 'Memória interna indisponível. Use Preparar memória, ou desmarque a gravação no ESP32.',
    'FLASH_FULL': 'Memória interna cheia. Exporte os registros antes de excluir arquivos.',
    'FLASH_WRITE': 'Falha de gravação na memória interna; o arquivo pode estar incompleto.',
    'FLASH_PREPARE_FAILED': 'Falha ao preparar LittleFS. Confira a partição da placa no Arduino IDE.',
    'LORA_TX_TIMEOUT': 'O rádio não concluiu o envio no prazo. A leitura USB continua.',
    'LORA_NOT_READY': 'LoRa indisponível. Verifique a montagem e reinicie a bancada.',
    'BUSY': 'A bancada já está adquirindo dados.',
    'RADIO_FILE_ACTIVE': 'Há um registro sendo enviado pelo LoRa. Aguarde ou cancele esse envio.',
}

class LiveApp:
    def __init__(self, root):
        self.root = root
        root.title('Bancada de empuxo | TG2 — v0.4.0')
        root.geometry('1240x820')
        root.minsize(1080, 720)
        self.events = queue.Queue(maxsize=12000)
        self.links = {}
        self.board = None
        self.writer = None
        self.acquisition = None
        self.timing = None
        self.reference = None
        self.preview = deque(maxlen=100)
        self.phase = 'IDLE'
        self.deadline = 0
        self.last_query = self.last_flush = self.last_draw = 0
        self.last_source = {'USB': 0, 'LORA': 0}
        self.invalid = 0
        self.closing = False
        self.download = None
        self.pending_download = None
        self.radio_transfer = None
        self.download_deadline = 0
        self.file_window = self.file_tree = None
        self.pending_end = None
        self.stop_requested_by = None
        self.viewing_file = False
        self.folder = Path.home() / 'TCC_Bancada' / 'ensaios'
        self.name = tk.StringVar(value='Teste célula 5 kg — sem calibração')
        self.duration = tk.StringVar(value='60')
        self.usb_port = tk.StringVar()
        self.rx_port = tk.StringVar(value='Sem estação')
        self.flash = tk.BooleanVar(value=True)
        self.source = tk.StringVar(value='USB da bancada')
        self.relative = tk.BooleanVar(value=True)
        self.status = tk.StringVar(value='DESCONECTADO • selecione o USB da bancada')
        self.detail = tk.StringVar(value='Teste principal por USB. O registro salvo pode ser enviado depois para outro computador via LoRa.')
        self.board_text = tk.StringVar(value='Bancada: —   |   HX711: —   |   Memória interna: —   |   Rádio: —')
        self.summary = tk.StringVar(value='Nenhum teste iniciado.')
        self.path_text = tk.StringVar(value=f'Dados: {self.folder}')
        self.reference_text = tk.StringVar(value='Referência sem carga: não definida')
        self.link_text = tk.StringVar(value='USB: sem dados   |   LoRa: sem dados')
        self.timing_text = tk.StringVar(value='O tempo do ensaio será informado pelo relógio do ESP32.')
        self.build_ui()
        self.refresh_ports()
        root.protocol('WM_DELETE_WINDOW', self.close)
        root.after(100, self.tick)

    @property
    def active(self):
        return self.writer is not None and not self.writer.closed

    def build_ui(self):
        style = ttk.Style(self.root)
        style.theme_use('clam')
        style.configure('TFrame', background='#edf2f7')
        style.configure('TLabel', background='#edf2f7', foreground='#152b46', font=('Segoe UI', 10))
        style.configure('TButton', padding=(8, 6), font=('Segoe UI', 10))
        style.configure('Accent.TButton', background='#155eaa', foreground='white')
        style.map('Accent.TButton', background=[('disabled', '#abb8c5'), ('active', '#104e8e')])
        self.root.configure(bg='#edf2f7')
        header = tk.Frame(self.root, bg='#152b46', padx=20, pady=12)
        header.pack(fill='x')
        tk.Label(header, text='BANCADA DE EMPUXO', bg='#152b46', fg='white', font=('Segoe UI', 19, 'bold')).pack(anchor='w')
        tk.Label(header, text='TG2  /  Aquisição por USB · Envio do registro por LoRa  /  v0.4.0', bg='#152b46', fg='#bdcfe2').pack(anchor='w', pady=(3, 0))
        tk.Label(self.root, text='CÉLULA DE 5 kg · SEM CALIBRAÇÃO · Leitura em contagens do HX711',
                 bg='#fff0ce', fg='#654600', pady=8, font=('Segoe UI', 11, 'bold')).pack(fill='x')
        connections = ttk.Frame(self.root, padding=(16, 10, 16, 6))
        connections.pack(fill='x')
        ttk.Label(connections, text='USB da bancada').grid(row=0, column=0, sticky='w')
        ttk.Label(connections, text='Estação neste PC (opcional)').grid(row=0, column=1, sticky='w', padx=(10, 0))
        self.usb_box = ttk.Combobox(connections, textvariable=self.usb_port, state='readonly', width=33)
        self.usb_box.grid(row=1, column=0, sticky='ew')
        self.rx_box = ttk.Combobox(connections, textvariable=self.rx_port, state='readonly', width=33)
        self.rx_box.grid(row=1, column=1, sticky='ew', padx=(10, 10))
        self.scan_button = ttk.Button(connections, text='Atualizar portas', command=self.refresh_ports)
        self.scan_button.grid(row=1, column=2, padx=4)
        self.connect_button = ttk.Button(connections, text='Conectar', command=self.connect)
        self.connect_button.grid(row=1, column=3, padx=4)
        self.disconnect_button = ttk.Button(connections, text='Desconectar', command=self.disconnect)
        self.disconnect_button.grid(row=1, column=4, padx=4)
        ttk.Label(connections, textvariable=self.board_text).grid(row=2, column=0, columnspan=5, sticky='w', pady=(7, 0))
        body = ttk.Frame(self.root, padding=(16, 8, 16, 4))
        body.pack(fill='both', expand=True)
        body.columnconfigure(1, weight=1)
        body.rowconfigure(0, weight=1)
        left = ttk.Frame(body, padding=(0, 0, 16, 0))
        left.grid(row=0, column=0, sticky='ns')
        ttk.Label(left, text='TESTE DE AQUISIÇÃO', font=('Segoe UI', 11, 'bold')).pack(anchor='w', pady=(0, 7))
        self.inputs = []
        for label, var in [('Nome do teste', self.name), ('Duração programada (s): 5 a 3600', self.duration)]:
            ttk.Label(left, text=label).pack(anchor='w', pady=(5, 3))
            entry = ttk.Entry(left, textvariable=var, width=31)
            entry.pack(fill='x')
            self.inputs.append(entry)
        ttk.Label(left, text='Observações').pack(anchor='w', pady=(7, 3))
        self.notes = tk.Text(left, width=28, height=3, wrap='word', font=('Segoe UI', 9), relief='solid', bd=1)
        self.notes.pack(fill='x')
        self.flash_check = ttk.Checkbutton(left, text='Gravar também no ESP32', variable=self.flash)
        self.flash_check.pack(anchor='w', pady=(8, 4))
        self.zero_button = ttk.Button(left, text='Definir referência sem carga', command=self.set_reference)
        self.zero_button.pack(fill='x', pady=3)
        ttk.Label(left, textvariable=self.reference_text, wraplength=250, font=('Segoe UI', 9)).pack(anchor='w', pady=(1, 6))
        self.start_button = ttk.Button(left, text='▶  Iniciar aquisição', command=self.start, style='Accent.TButton')
        self.start_button.pack(fill='x', pady=3)
        self.stop_button = ttk.Button(left, text='■  Encerrar aquisição', command=self.stop)
        self.stop_button.pack(fill='x', pady=3)
        self.radio_button = ttk.Button(left, text='Telemetria LoRa opcional', command=self.toggle_radio)
        self.radio_button.pack(fill='x', pady=3)
        self.prepare_button = ttk.Button(left, text='Preparar memória do ESP32…', command=self.prepare_flash)
        self.prepare_button.pack(fill='x', pady=3)
        self.files_button = ttk.Button(left, text='Arquivos no ESP32…', command=self.show_files)
        self.files_button.pack(fill='x', pady=3)
        self.open_button = ttk.Button(left, text='Abrir teste salvo…', command=self.open_session)
        self.open_button.pack(fill='x', pady=3)
        self.folder_button = ttk.Button(left, text='Escolher pasta de dados…', command=self.choose_folder)
        self.folder_button.pack(fill='x', pady=3)
        right = ttk.Frame(body)
        right.grid(row=0, column=1, sticky='nsew')
        cards = ttk.Frame(right)
        cards.pack(fill='x', pady=(0, 8))
        self.values = []
        for i, label in enumerate(('LEITURA BRUTA', 'VARIAÇÃO À REFERÊNCIA', 'TEMPO DO ENSAIO (s)', 'RSSI LoRa (dBm)')):
            cards.columnconfigure(i, weight=1)
            card = tk.Frame(cards, bg='white', padx=10, pady=8)
            card.grid(row=0, column=i, sticky='ew', padx=(0, 5))
            tk.Label(card, text=label, bg='white', fg='#53657a', font=('Segoe UI', 8)).pack(anchor='w')
            var = tk.StringVar(value='—')
            tk.Label(card, textvariable=var, bg='white', fg='#152b46', font=('Segoe UI', 18, 'bold')).pack(anchor='w')
            self.values.append(var)
        ttk.Label(right, textvariable=self.timing_text, wraplength=850).pack(anchor='w', pady=(0, 5))
        options = ttk.Frame(right)
        options.pack(fill='x', pady=(0, 6))
        ttk.Label(options, text='Exibir:').pack(side='left')
        ttk.Combobox(options, textvariable=self.source, values=('LoRa da estação', 'USB da bancada'),
                     state='readonly', width=20).pack(side='left', padx=6)
        ttk.Checkbutton(options, text='Subtrair referência', variable=self.relative).pack(side='left', padx=4)
        self.fig = Figure(figsize=(7, 4), dpi=100, layout='constrained')
        self.ax = self.fig.add_subplot(111)
        self.ax.grid(True, alpha=0.18)
        self.ax.set_xlabel('Tempo desde o início da aquisição (s)')
        self.ax.set_ylabel('Leitura bruta (contagens)')
        self.ax.set_title('Aguardando dados reais', loc='left', fontsize=12)
        self.line, = self.ax.plot([], [], color='#1769aa', linewidth=1.3, marker='.', markersize=3)
        self.canvas = FigureCanvasTkAgg(self.fig, master=right)
        self.canvas.get_tk_widget().pack(fill='both', expand=True)
        NavigationToolbar2Tk(self.canvas, right, pack_toolbar=False).pack(fill='x')
        ttk.Label(right, textvariable=self.summary, wraplength=850).pack(anchor='w', pady=(5, 0))
        ttk.Label(right, textvariable=self.link_text, font=('Segoe UI', 9)).pack(anchor='w', pady=(4, 0))
        footer = ttk.Frame(self.root, padding=(16, 7, 16, 10))
        footer.pack(fill='x')
        ttk.Label(footer, textvariable=self.status, font=('Segoe UI', 11, 'bold')).pack(anchor='w')
        ttk.Label(footer, textvariable=self.detail, wraplength=1160).pack(anchor='w', pady=2)
        ttk.Label(footer, textvariable=self.path_text, font=('Segoe UI', 9), wraplength=1160).pack(anchor='w')
        self.update_controls()

    def refresh_ports(self):
        if self.links:
            return
        self.port_map = {f'{device} — {desc}': device for device, desc in ports()}
        names = list(self.port_map)
        self.usb_box.configure(values=names)
        self.rx_box.configure(values=['Sem estação', *names])
        if self.usb_port.get() not in names:
            self.usb_port.set(names[0] if names else '')
        if self.rx_port.get() not in names:
            self.rx_port.set('Sem estação')

    def connect(self):
        try:
            usb = self.port_map.get(self.usb_port.get())
            rx = self.port_map.get(self.rx_port.get())
            if not usb:
                raise ValueError('Selecione a porta USB da bancada.')
            if usb == rx:
                raise ValueError('Bancada e estação precisam de portas COM diferentes.')
            self.links['USB'] = SerialLink(usb, 'USB', self.events)
            if rx:
                self.links['LORA'] = SerialLink(rx, 'LORA', self.events)
            self.source.set('USB da bancada')
            self.preview.clear()
            self.reference = None
            self.viewing_file = False
            self.acquisition = None
            self.timing = None
            self.reference_text.set('Referência sem carga: não definida')
            self.last_source = {'USB': 0, 'LORA': 0}
            self.status.set('CONECTANDO • aguardando resposta da bancada')
            self.detail.set('Aguarde alguns segundos. Se não responder, confira a porta e o firmware da bancada v0.4.0.')
            self.last_query = monotonic()  # permite que eventual reset ao abrir termine
        except Exception as exc:
            self.disconnect()
            messagebox.showerror('Falha na conexão', f'{exc}\n\nFeche o Monitor Serial do Arduino e outros programas que usam essas portas.', parent=self.root)
        self.update_controls()

    def disconnect(self):
        if self.active or self.download or self.pending_download or self.radio_transfer:
            messagebox.showinfo('Operação em andamento', 'Encerre a aquisição ou aguarde a transferência antes de desconectar.', parent=self.root)
            return
        for link in self.links.values():
            link.close()
        self.links.clear()
        while True:
            try: self.events.get_nowait()
            except queue.Empty: break
        self.board = None
        self.radio_transfer = None
        self.preview.clear()
        self.status.set('DESCONECTADO')
        self.board_text.set('Bancada: —   |   HX711: —   |   Memória interna: —   |   Rádio: —')
        self.update_controls()

    def send(self, command):
        link = self.links.get('USB')
        if not link:
            raise OSError('Conecte a porta USB da bancada.')
        link.send(command)
        if self.active:
            self.writer.event('COMMAND', command)

    def set_reference(self):
        recent = [raw for t, raw, flag in self.preview if monotonic() - t <= 3 and flag == 'OK']
        if len(recent) < 10 or not self.preview or monotonic() - self.preview[-1][0] > 1:
            messagebox.showinfo('Aguarde as leituras', 'Deixe a célula sem carga por alguns segundos. São necessárias 10 leituras recentes válidas.', parent=self.root)
            return
        self.reference = round(statistics.mean(recent[-20:]))
        self.reference_text.set(f'Referência: {self.reference} contagens\nApenas zero relativo; não é calibração.')

    def start(self):
        if self.active:
            return
        try:
            if not self.board or self.board['state'] != 'IDLE':
                raise ValueError('Aguarde a bancada confirmar que está pronta.')
            if self.flash.get() and self.board['flash'] != 'OK':
                raise ValueError(ERRORS['FLASH_NOT_READY'])
            duration = int(self.duration.get())
            if not 5 <= duration <= 3600:
                raise ValueError('Escolha um limite inteiro de 5 a 3600 segundos.')
            sid = uuid4().hex[:16]
            writer = LiveWriter(self.folder, sid, self.name.get(), self.notes.get('1.0', 'end-1c'),
                self.reference, duration, {k: v.port.port for k, v in self.links.items()}, self.flash.get())
            self.writer = writer
            self.acquisition = Acquisition(sid)
            self.timing = AcquisitionTiming(sid, duration)
            self.viewing_file = False
            self.invalid = 0
            self.phase = 'STARTING'
            self.deadline = monotonic() + 7
            self.pending_end = None
            self.stop_requested_by = None
            self.last_source = {'USB': 0, 'LORA': 0}
            self.send(f'START {sid} {duration} {int(self.flash.get())}')
            self.status.set('INÍCIO SOLICITADO • aguardando confirmação do ESP32')
            self.detail.set('O botão inicia somente a aquisição e o registro dos dados.')
            self.path_text.set(f'Dados: {writer.folder}')
        except Exception as exc:
            if self.active:
                self.finish_local('start_error', False, str(exc))
            messagebox.showerror('Não foi possível iniciar', str(exc), parent=self.root)
        self.update_controls()

    def stop(self, origin='user'):
        sid = self.acquisition.session if self.active else (self.board or {}).get('session', '-')
        if sid == '-': return
        try:
            if self.active and self.stop_requested_by is None and origin != 'recover_confirmation':
                self.stop_requested_by = origin
                self.writer.event('CONTROL', f'STOP_REQUESTED_BY;{origin}')
            self.send(f'STOP {sid}')
            if self.active:
                self.phase = 'STOPPING'
                self.deadline = monotonic() + 6
            self.status.set('ENCERRAMENTO SOLICITADO • aguardando confirmação')
        except Exception as exc:
            if self.active:
                self.finish_local('stop_unconfirmed', False, str(exc))
            self.detail.set('Sem confirmação da bancada. A aquisição interna pode continuar até o limite configurado.')
        self.update_controls()

    def toggle_radio(self):
        if self.board:
            try: self.send('RADIO OFF' if self.board['radio'] == 'ON' else 'RADIO ON')
            except Exception as exc: self.detail.set(str(exc))

    def prepare_flash(self):
        if self.board and self.board['flash'] == 'OK':
            messagebox.showinfo('Memória pronta', 'LittleFS já está disponível. Os arquivos existentes foram preservados.', parent=self.root)
            return
        if not messagebox.askyesno('Preparar memória interna',
            'Se a partição de arquivos não puder ser montada, ela será formatada. Isso pode apagar arquivos antigos dessa partição.\n\nPreparar a memória de arquivos do ESP32?', parent=self.root):
            return
        try:
            self.send('PREPARE_FLASH CONFIRM')
            self.status.set('PREPARANDO MEMÓRIA • aguarde a resposta')
        except Exception as exc: self.detail.set(str(exc))

    def handle_line(self, source, line, received):
        if self.active:
            self.writer.event(source, line)
        if source == 'USB' and self.handle_file_line(line):
            return
        packet = parse_line(line)
        if packet is not None:
            if self.active and self.acquisition.add(source, packet):
                self.writer.append(source, packet)
                self.last_source[source] = received
            return
        if source != 'USB':
            if line.startswith("Received packet '"):
                self.detail.set('A estação recebeu outro formato. Para receber arquivos completos, use RECEPTOR_LORA.bat na porta da estação.')
            return
        p = line.split(';')
        if p[0] == 'CLOCK':
            if self.active and self.timing and self.timing.accept(line, received):
                self.writer.meta['timing'] = self.timing.snapshot()
                if self.phase == 'STARTING' and self.timing.phase == 'RECORDING':
                    self.mark_started()  # relógio da sessão também comprova que iniciou
        elif p[0] == 'LORA_FILE':
            if len(p) >= 3 and p[1] == 'START':
                self.radio_transfer = p[2]
                self.status.set('ENVIANDO REGISTRO PELO LoRa')
                self.detail.set('O arquivo só é confirmado como recebido no notebook da estação.')
            elif len(p) == 5 and p[1] == 'PROGRESS':
                self.status.set(f'ENVIO LoRa • {p[3]} / {p[4]} blocos transmitidos localmente')
            elif len(p) >= 3 and p[1] in ('SENT', 'CANCELLED'):
                self.radio_transfer = None
                self.status.set('ENVIO LOCAL ENCERRADO' if p[1] == 'SENT' else 'ENVIO LoRa CANCELADO')
                self.detail.set('Confira RECEBIDO E VERIFICADO no notebook receptor. Se faltarem blocos, envie novamente o mesmo registro.' if p[1] == 'SENT' else 'O registro permanece no ESP32 e pode ser enviado novamente.')
        elif p[0] == 'PREVIEW' and len(p) == 4:
            raw = int(p[2])
            if not -8388608 <= raw <= 8388607 or p[3] not in ('OK', 'SAT'):
                raise ValueError('Prévia do HX711 inválida.')
            self.preview.append((received, raw, p[3]))
        elif p[0] == 'STATUS' and len(p) == 8:
            self.board = dict(state=p[1], session=p[2], count=int(p[3]), flash=p[4], free=int(p[5]), radio=p[6], hx=p[7])
            if p[1] == 'TRANSFER':
                self.radio_transfer = self.radio_transfer or 'ACTIVE'
            elif self.radio_transfer:
                self.radio_transfer = None
                self.detail.set('A bancada não indica transferência ativa. Confira o resultado no notebook receptor.')
            self.board_text.set(f'Bancada: {p[1]}   |   HX711: {p[7]}   |   Memória: {p[4]} ({int(p[5]) // 1024} KiB livres)   |   Rádio: {p[6]}')
            if not self.active and not self.viewing_file and self.acquisition is None:
                states = {'IDLE': 'CONECTADO • pronto para teste', 'RECORDING': 'BANCADA ADQUIRINDO • pode encerrar pelo botão',
                          'TRANSFER': 'ENVIANDO REGISTRO PELO LoRa • aguarde ou cancele na janela de arquivos'}
                self.status.set(states.get(p[1], 'ESTADO DA BANCADA: ' + p[1]))
            elif p[1] == 'RECORDING' and p[2] == self.acquisition.session and self.phase == 'STARTING':
                self.mark_started()
            elif self.phase == 'RUNNING' and p[1] == 'IDLE':
                if p[2] == self.acquisition.session:
                    self.stop('recover_confirmation')  # solicita novamente a confirmação perdida
                else:
                    self.finish_local('board_reset', False, 'ESP32_REINICIADO')
        elif p[:2] == ['ACK', 'START'] and len(p) == 3:
            if self.active and p[2] == self.acquisition.session and self.phase == 'STARTING':
                self.mark_started()
        elif p[:2] == ['ACK', 'STOP'] and len(p) == 5:
            if self.active and p[2] == self.acquisition.session:
                # Um pacote LoRa pode chegar pouco depois da confirmação USB.
                self.pending_end = (p[3], int(p[4]))
                self.phase = 'DRAINING'
                self.deadline = monotonic() + 1.0
                self.status.set('AQUISIÇÃO ENCERRADA • recebendo últimos pacotes')
        elif p[:2] == ['ACK', 'DELETE_ALL'] and len(p) == 4:
            self.status.set(('EXCLUSÃO CONCLUÍDA' if p[2] == 'OK' else 'EXCLUSÃO PARCIAL') + f' • {int(p[3])} testes apagados')
            self.detail.set('Cópias no computador preservadas.' if p[2] == 'OK' else 'Houve falha ao excluir um registro. Atualize a lista e confira os restantes.')
            self.list_files()
        elif p[:2] == ['ACK', 'RADIO'] and len(p) == 3:
            if self.board: self.board['radio'] = p[2]
            self.detail.set('LoRa pausado; aquisição USB e memória continuam.' if p[2] == 'OFF' else 'Transmissão LoRa habilitada.')
        elif p[:2] == ['ACK', 'FLASH']:
            self.detail.set('Memória interna preparada. A gravação começa ao iniciar a aquisição.')
        elif p[0] == 'ERR':
            reason = p[1] if len(p) > 1 else line
            self.detail.set(ERRORS.get(reason, f'Bancada informou: {reason}'))
            if self.phase == 'STARTING' and self.active:
                self.finish_local('start_rejected', False, reason)
        elif p[:2] == ['EVT', 'READY'] and self.active:
            self.finish_local('board_reset', False, 'ESP32_REINICIADO')

    def mark_started(self):
        self.phase = 'RUNNING'
        self.writer.update(state='recording', start_confirmed=True)
        self.status.set('ADQUIRINDO • dados brutos, sem calibração')
        self.detail.set('O cronômetro independe da carga. Retirar a carga não encerra o teste; uma falha será indicada como interrupção.')

    def finish_local(self, state, confirmed, reason, count=None):
        if not self.active:
            return
        stats = self.acquisition.stats()
        local_cause = self.stop_requested_by
        if local_cause == 'start_confirmation_timeout':
            state = 'start_confirmation_timeout'
        timing = self.timing.snapshot(confirmed) if self.timing else {}
        elapsed = timing.get('last_elapsed_us')
        early_limit = bool(reason == 'LIMIT' and timing.get('duration_confirmed') and elapsed < timing['accepted_duration_us'])
        if early_limit:
            state = 'timing_error'
        time_text = f'{elapsed / 1e6:.2f} / {self.timing.requested_s} s' if elapsed is not None else 'tempo não informado'
        try:
            self.writer.meta.update(invalid_messages=self.invalid,
                timing=timing,
                stop_requested_by=local_cause,
                last_sample_us={k: t.samples[-1].t_us if t.samples else None for k, t in self.acquisition.traces.items()},
                queue_dropped={key: link.dropped for key, link in self.links.items() if hasattr(link, 'dropped')},
                usb_missing_from_board_count=None if count is None else count - stats['usb']['count'])
            self.writer.finish(state, stats, confirmed, reason, count)
        except OSError as exc:
            self.status.set('FALHA AO SALVAR • preserve os arquivos existentes')
            self.detail.set(str(exc))
        else:
            if confirmed:
                if reason == 'LIMIT':
                    reached = timing.get('duration_confirmed') and elapsed >= timing['accepted_duration_us']
                    self.status.set('INCONSISTÊNCIA DE TEMPO • ' + time_text if early_limit else
                        (f'DURAÇÃO ATINGIDA • {time_text}' if reached else 'ENCERRADO PELO LIMITE • duração não confirmada'))
                elif reason == 'MANUAL':
                    if local_cause == 'start_confirmation_timeout':
                        self.status.set(f'INTERROMPIDO • início não confirmado pelo USB • {time_text}')
                    else:
                        self.status.set(f'ENCERRADO POR COMANDO • {time_text}')
                else:
                    self.status.set(f'INTERROMPIDO • {time_text} • {reason}')
                counts = f'ESP32: {count} leituras; PC: {stats["usb"]["count"]} por USB e {stats["lora"]["count"]} pelo LoRa.'
                self.detail.set((ERRORS.get(reason, f'Falha informada: {reason}.') + ' ' if reason not in ('MANUAL', 'LIMIT') else '') + counts)
                if local_cause == 'start_confirmation_timeout':
                    self.detail.set('O aplicativo solicitou a parada porque não recebeu confirmação de início em 7 s. ' + counts)
                if early_limit:
                    self.detail.set('O ESP32 informou fim por limite antes da duração programada. Preserve ensaio.json e eventos.jsonl para análise. ' + counts)
            else:
                self.status.set(f'INTERRUPÇÃO • encerramento não confirmado • {reason}')
                self.detail.set(ERRORS.get(reason, 'O estado final da bancada não foi confirmado. O registro interno pode continuar até o limite de duração.'))
        self.phase = 'IDLE'
        self.pending_end = None
        if self.closing:
            if confirmed:
                self.disconnect()
                self.root.destroy()
                return
            else:
                self.closing = False
        self.update_controls()

    def update_controls(self):
        connected = bool(self.links)
        board_ready = self.board is not None and 'USB' in self.links
        busy = self.active or bool(self.download) or bool(self.pending_download) or bool(self.radio_transfer)
        board_busy = board_ready and self.board['state'] != 'IDLE'
        idle = board_ready and not busy and not board_busy
        for widget in (self.usb_box, self.rx_box): widget.configure(state='disabled' if connected else 'readonly')
        for widget in (self.scan_button, self.connect_button): widget.configure(state='disabled' if connected else 'normal')
        self.disconnect_button.configure(state='normal' if connected and not busy else 'disabled')
        self.start_button.configure(state='normal' if idle else 'disabled')
        self.zero_button.configure(state='normal' if idle else 'disabled')
        self.stop_button.configure(state='normal' if (self.active and self.phase != 'DRAINING') or (board_ready and self.board['state'] == 'RECORDING') else 'disabled')
        self.radio_button.configure(state='normal' if board_ready and not self.radio_transfer else 'disabled')
        if board_ready:
            self.radio_button.configure(text='Pausar telemetria ao vivo' if self.board['radio'] == 'ON' else 'Ativar telemetria ao vivo')
        for widget in (self.prepare_button, self.files_button): widget.configure(state='normal' if idle else 'disabled')
        for widget in (self.open_button, self.folder_button, self.flash_check, *self.inputs, self.notes):
            widget.configure(state='disabled' if busy or board_busy else 'normal')

    def draw(self):
        source = 'LORA' if self.source.get().startswith('LoRa') else 'USB'
        samples = self.acquisition.traces[source].samples if self.acquisition else []
        offset = self.reference if self.relative.get() and self.reference is not None else 0
        is_relative = self.relative.get() and self.reference is not None
        self.ax.set_ylabel('Variação (contagens)' if is_relative else 'Leitura bruta (contagens)')
        self.ax.set_title(('LoRa recebido pela estação' if source == 'LORA' else 'USB direto da bancada') + ' • SEM CALIBRAÇÃO', loc='left', fontsize=11)
        self.line.set_linestyle('None' if source == 'LORA' else '-')
        self.line.set_markersize(5 if source == 'LORA' else 3)
        if samples:
            last = samples[-1]
            step = max(1, len(samples) // 4000)
            plot = samples[::step]
            if plot[-1] != last: plot = [*plot, last]
            # Interrompe a linha em lacunas do transporte. Não interpola amostras perdidas.
            x, y = [], []
            previous = None
            for p in plot:
                index = p.seq if source == 'USB' else p.packet_seq
                if previous is not None and step == 1 and index != previous + 1:
                    x.append(float('nan')); y.append(float('nan'))
                x.append(p.t_us / 1e6); y.append(float('nan') if p.flag == 'SAT' else p.raw - offset)
                previous = index
            self.line.set_data(x, y)
            self.ax.relim(); self.ax.autoscale_view()
            self.values[0].set(str(last.raw) + (' SAT' if last.flag == 'SAT' else ''))
            self.values[1].set('—' if self.reference is None else str(last.raw - self.reference))
            self.values[3].set('—' if last.rssi is None else str(last.rssi))
        else:
            self.line.set_data([], [])
            for value in self.values: value.set('—')
            if not self.active and self.preview and monotonic() - self.preview[-1][0] < 2 and source == 'USB':
                _, raw, flag = self.preview[-1]
                self.values[0].set(str(raw) + (' SAT' if flag == 'SAT' else ''))
                self.values[1].set('—' if self.reference is None else str(raw - self.reference))
        # Usa exclusivamente o tempo recebido do ESP32, nunca o índice da amostra.
        elapsed = self.timing.elapsed_us if self.timing else None
        self.values[2].set(f'{elapsed / 1e6:.2f}' if elapsed is not None else '—')
        last_sample = f'{samples[-1].t_us / 1e6:.3f} s' if samples else 'nenhuma'
        if self.timing and elapsed is not None:
            age = monotonic() - self.timing.received_at if self.timing.received_at is not None else 0
            suffix = ' • aguardando atualização USB' if self.active and self.timing.phase == 'RECORDING' and age > 2 else ''
            self.timing_text.set(f'Programado: {self.timing.requested_s} s · Última amostra {source}: {last_sample}{suffix}')
        else:
            self.timing_text.set(f'Tempo do ensaio não informado · Última amostra {source}: {last_sample}' +
                (' · Use o firmware v0.4.0.' if self.active else ''))
        if self.acquisition:
            stats = self.acquisition.stats()
            received = self.acquisition.traces['LORA'].samples
            self.values[3].set(str(received[-1].rssi) if received and received[-1].rssi is not None else '—')
            usb, radio = stats['usb'], stats['lora']
            self.summary.set(f'USB: {usb["count"]} leituras · LoRa: {radio["count"]} pacotes · Coincidem: {stats["verified"]} · Divergem: {stats["mismatches"]}\n'
                f'Lacunas USB: {usb["gaps"]} · Lacunas de pacotes LoRa: {radio["gaps"]} · Inválidos: {self.invalid} · Saturados: {usb["saturated"]}/{radio["saturated"]}')
        if self.active:
            labels = []
            for key in ('USB', 'LORA'):
                stamp = self.last_source[key]
                age = monotonic() - stamp if stamp else None
                labels.append(f'{key}: ' + ('sem dados deste ensaio' if age is None else f'última leitura há {age:.1f} s'))
            self.link_text.set('   |   '.join(labels))
        else:
            self.link_text.set('Registro parado / arquivo aberto. Os números acima não indicam um enlace ativo.')
        self.canvas.draw_idle()

    def tick(self):
        try:
            for _ in range(1500):
                try: source, kind, value, received = self.events.get_nowait()
                except queue.Empty: break
                if kind == 'line':
                    try: self.handle_line(source, value, received)
                    except (ValueError, UnicodeError) as exc:
                        self.invalid += 1
                        self.detail.set(f'Dado descartado: {exc}')
                elif kind == 'error':
                    self.detail.set(f'{source}: conexão perdida — {value}')
                    link = self.links.pop(source, None)
                    if link: link.close()
                    if source == 'USB':
                        self.board = None
                        self.radio_transfer = None
                        self.abort_download('USB desconectado')
                        if self.active: self.finish_local('usb_disconnected', False, 'USB_DESCONECTADO')
                else:
                    self.invalid += 1
            now = monotonic()
            if 'USB' in self.links and now - self.last_query >= 2:
                self.send('STATUS')
                self.last_query = now
            if self.active and now - self.last_flush >= 1:
                self.writer.flush()
                self.last_flush = now
            if self.active and self.phase in ('STARTING', 'STOPPING', 'DRAINING') and now >= self.deadline:
                if self.phase == 'STARTING': self.stop('start_confirmation_timeout')
                elif self.phase == 'STOPPING': self.finish_local('stop_timeout', False, 'SEM_CONFIRMACAO_USB')
                else:
                    reason, count = self.pending_end
                    self.finish_local('stopped' if reason in ('MANUAL', 'LIMIT') else 'hardware_error', True, reason, count)
                    if self.closing: return
            if (self.download or self.pending_download) and now > self.download_deadline:
                self.abort_download('Tempo esgotado na transferência. Arquivo parcial preservado.')
            for key, link in self.links.items():
                if link.dropped:
                    self.detail.set(f'Fila do PC excedida em {key}: {link.dropped} mensagens não processadas. Confira lacunas e registro interno.')
            if now - self.last_draw >= 0.25:
                self.draw(); self.last_draw = now
            self.update_controls()
        except (OSError, RuntimeError, queue.Full) as exc:
            if self.active:
                try: self.send(f'STOP {self.acquisition.session}')
                except Exception: pass
                self.finish_local('pc_error', False, str(exc))
            self.detail.set(f'Falha no computador: {exc}')
        if self.root.winfo_exists(): self.root.after(100, self.tick)

    def choose_folder(self):
        path = filedialog.askdirectory(title='Pasta dos próximos ensaios', parent=self.root)
        if path:
            self.folder = Path(path)
            self.path_text.set(f'Dados: {self.folder}')

    def open_session(self):
        path = filedialog.askopenfilename(title='Abrir registro bruto', initialdir=str(self.folder),
            filetypes=[('Ensaio e registro ESP32', '*.json *.log'), ('Todos', '*.*')], parent=self.root)
        if path: self.open_path(Path(path))

    def open_path(self, path):
        try:
            meta, acquisition = load_flash(path) if path.suffix == '.log' else load_live(path)
            self.acquisition = acquisition
            self.timing = AcquisitionTiming.from_metadata(acquisition.session, meta)
            self.viewing_file = True
            self.reference = meta.get('reference_counts')
            self.reference_text.set('Referência do arquivo: ' + ('não definida' if self.reference is None else str(self.reference)))
            self.name.set(meta.get('test_name', 'Registro salvo'))
            self.source.set('LoRa da estação' if acquisition.traces['LORA'].samples else 'USB da bancada')
            self.path_text.set(f'Arquivo: {path}')
            self.status.set('ARQUIVO ABERTO • ' + meta['state'])
            self.detail.set('Dados brutos preservados. Sem conversão para força ou massa.')
            if not meta.get('stop_confirmed'):
                self.detail.set('Registro sem encerramento confirmado. Pode estar incompleto; dados existentes foram preservados.')
            self.draw()
        except (ValueError, OSError, KeyError, TypeError) as exc:
            messagebox.showerror('Não foi possível abrir', str(exc), parent=self.root)

    def show_files(self):
        if self.file_window and self.file_window.winfo_exists():
            self.file_window.lift(); return
        win = self.file_window = tk.Toplevel(self.root)
        win.title('Registros na memória do ESP32'); win.geometry('800x430')
        ttk.Label(win, text='Registros locais. Baixar não exclui o arquivo do ESP32.', padding=12).pack(anchor='w')
        self.file_tree = ttk.Treeview(win, columns=('id', 'size'), show='headings', selectmode='browse')
        self.file_tree.heading('id', text='Identificador do ensaio'); self.file_tree.heading('size', text='Bytes')
        self.file_tree.column('id', width=330); self.file_tree.column('size', width=100)
        self.file_tree.pack(fill='both', expand=True, padx=12, pady=5)
        row = ttk.Frame(win, padding=12); row.pack(fill='x')
        ttk.Button(row, text='Atualizar', command=self.list_files).pack(side='left')
        ttk.Button(row, text='Baixar selecionado', command=self.download_selected).pack(side='left', padx=8)
        ttk.Button(row, text='Enviar por LoRa', command=self.send_selected_radio).pack(side='left', padx=4)
        ttk.Button(row, text='Cancelar envio LoRa', command=self.cancel_radio_file).pack(side='left', padx=4)
        ttk.Button(row, text='Excluir selecionado…', command=self.delete_selected).pack(side='right')
        ttk.Button(win, text='Apagar TODOS os testes do ESP32…', command=self.delete_all_tests).pack(pady=5)
        self.list_files()

    def delete_all_tests(self):
        if self.active or self.download or self.pending_download or self.radio_transfer: return
        if not self.board or self.board['state'] != 'IDLE': return
        if messagebox.askyesno('Apagar todos os testes',
            'Excluir permanentemente TODOS os registros de ensaio do ESP32?\nAs cópias salvas no computador serão mantidas.',
            parent=self.file_window):
            try: self.send('DELETE_ALL CONFIRM')
            except Exception as exc: self.detail.set(str(exc))

    def list_files(self):
        if self.active or self.download or self.pending_download or self.radio_transfer: return
        if self.file_tree:
            self.file_tree.delete(*self.file_tree.get_children())
        try: self.send('LIST')
        except Exception as exc: self.detail.set(str(exc))

    def selected_file(self):
        selected = self.file_tree.selection() if self.file_tree else []
        return self.file_tree.item(selected[0], 'values')[0] if selected else None

    def download_selected(self):
        if self.active or self.download or self.pending_download or self.radio_transfer: return
        sid = self.selected_file()
        if not sid: return
        destination = filedialog.askdirectory(title='Salvar cópia do registro interno', initialdir=str(self.folder), parent=self.file_window)
        if not destination: return
        self.pending_download = (sid, Path(destination) / f'esp32_{sid}_{uuid4().hex[:6]}.log')
        self.download_deadline = monotonic() + 10
        try: self.send(f'GET {sid}')
        except Exception as exc: self.abort_download(str(exc))

    def delete_selected(self):
        if self.active or self.download or self.pending_download or self.radio_transfer: return
        sid = self.selected_file()
        if sid and messagebox.askyesno('Excluir registro interno', f'Excluir somente o ensaio {sid} do ESP32?\nA exclusão não pode ser desfeita. Baixe uma cópia primeiro.', parent=self.file_window):
            try:
                self.send(f'DELETE {sid}')
                self.root.after(700, self.list_files)
            except Exception as exc: self.detail.set(str(exc))

    def send_selected_radio(self):
        if self.active or self.download or self.pending_download or self.radio_transfer: return
        sid = self.selected_file()
        if not sid: return
        try:
            self.send(f'LORASEND {sid}')
            self.status.set('ENVIO SOLICITADO • aguardando o ESP32')
            self.detail.set('Deixe RECEPTOR_LORA.bat conectado à estação no notebook que receberá os dados.')
        except Exception as exc: self.detail.set(str(exc))

    def cancel_radio_file(self):
        try: self.send('LORACANCEL')
        except Exception as exc: self.detail.set(str(exc))

    def handle_file_line(self, line):
        p = line.split(';')
        if p[0] == 'FILE_INFO' and len(p) == 3:
            if self.file_tree and self.file_tree.winfo_exists():
                self.file_tree.insert('', 'end', values=(p[1], int(p[2])))
        elif p[0] == 'FILE_BEGIN' and len(p) == 3:
            if self.pending_download and p[1] == self.pending_download[0]:
                sid, path = self.pending_download
                partial = path.with_suffix('.log.partial')
                self.download = dict(sid=sid, path=path, partial=partial, file=partial.open('xb'), expected=int(p[2]), count=0)
                self.pending_download = None
                self.download_deadline = monotonic() + 15
        elif line.startswith('FILE_DATA;'):
            if self.download:
                data = (line[len('FILE_DATA;'):] + '\n').encode('ascii')
                self.download['file'].write(data)
                self.download['count'] += len(data)
                self.download_deadline = monotonic() + 15
                self.detail.set(f'Baixando memória: {self.download["count"]} / {self.download["expected"]} bytes')
        elif p[0] == 'FILE_END' and len(p) == 3:
            if self.download:
                d = self.download
                d['file'].flush(); os.fsync(d['file'].fileno()); d['file'].close()
                self.download = None
                if p[1] != d['sid'] or int(p[2]) != d['expected'] or d['count'] != d['expected']:
                    self.detail.set(f'Transferência incompleta; mantido arquivo parcial: {d["partial"]}')
                else:
                    try: load_flash(d['partial'])
                    except (ValueError, OSError) as exc:
                        self.detail.set(f'Registro inválido: {exc}. Cópia parcial preservada.')
                    else:
                        os.replace(d['partial'], d['path'])
                        self.path_text.set(f'Registro baixado: {d["path"]}')
                        self.detail.set('Cópia recebida e conferida por tamanho, sequência e CRC. O original permanece no ESP32.')
        elif p[0] != 'FILE_LIST_END':
            return False
        return True

    def abort_download(self, reason):
        if self.download:
            self.download['file'].close()
            self.download = None
        if self.pending_download:
            self.pending_download = None
        self.detail.set(reason)

    def close(self):
        if self.radio_transfer:
            messagebox.showinfo('Envio LoRa em andamento', 'Aguarde terminar ou use Cancelar envio LoRa na janela de arquivos.', parent=self.root)
            return
        if self.active:
            if messagebox.askyesno('Encerrar aquisição', 'Solicitar encerramento da aquisição e fechar após a confirmação?', parent=self.root):
                self.closing = True
                self.stop('window_close')
            return
        if self.download or self.pending_download:
            messagebox.showinfo('Transferência', 'Aguarde o download terminar antes de fechar.', parent=self.root)
            return
        self.disconnect()
        self.root.destroy()

def main():
    root = tk.Tk()
    LiveApp(root)
    root.mainloop()
