"""Bancada de empuxo — versão 04: perfis de calibração por célula de carga.

Execute com: python main.py
Os próximos componentes serão acrescentados em ``self.area_principal``.
"""

import tkinter as tk
from datetime import date
from time import perf_counter
from tkinter import font, ttk

from calendario import Calendario
from confirmacao_motor import ConfirmacaoMotor
from componentes import PainelRolavel
from memoria import AbaMemoria
from dados_memoria import MemoriaDemonstracao
from perfis_calibracao import BancoPerfis
from calibracao import AbaCalibracao


VERSAO = "04"


def formatar_tempo(segundos):
    """Minutos, segundos e décimos, sem arredondar para um instante futuro."""
    decimos = max(0, int(segundos * 10))
    minutos, restante = divmod(decimos, 600)
    segundos, decimal = divmod(restante, 10)
    return f"{minutos:02d}:{segundos:02d}.{decimal}"


# Todas as cores ficam aqui para facilitar os ajustes durante o projeto.
TEMAS = {
    "escuro": {
        "fundo": "#101419",
        "superficie": "#171D25",
        "texto": "#F1F5F9",
        "secundario": "#A8B4C5",
        "borda": "#303B4A",
        "botao": "#1C2430",
        "hover": "#2A3748",
        "destaque": "#315FBB",
        "destaque_hover": "#3D6ECD",
        "foco": "#AAC8FF",
        "campo": "#10161E",
        "motor": "#FF939C",
        "motor_fundo": "#2D1B23",
        "motor_hover": "#45222D",
        "motor_borda": "#D85B6B",
        "confirmar_motor": "#B42338",
        "confirmar_motor_hover": "#CE3047",
        "conectado": "#7CD7AB",
        "curva": "#7CB6FF",
        "desativado": "#768496",
    },
    "claro": {
        "fundo": "#EEF2F6",
        "superficie": "#FFFFFF",
        "texto": "#182536",
        "secundario": "#526277",
        "borda": "#CED7E3",
        "botao": "#F8FAFC",
        "hover": "#E2E9F2",
        "destaque": "#2854AA",
        "destaque_hover": "#1D438E",
        "foco": "#173873",
        "campo": "#F8FAFC",
        "motor": "#A61F33",
        "motor_fundo": "#FFF3F4",
        "motor_hover": "#FFE2E6",
        "motor_borda": "#C64356",
        "confirmar_motor": "#B42338",
        "confirmar_motor_hover": "#911B2D",
        "conectado": "#176A45",
        "curva": "#2854AA",
        "desativado": "#778393",
    },
}


class BancadaApp(tk.Tk):
    """Janela única que receberá as funções da bancada, passo a passo."""

    def __init__(self, caminho_perfis=None):
        super().__init__()
        self.withdraw()  # Exibe a janela depois de aplicar as cores iniciais.
        self.title(f"Bancada de empuxo | Etapa {VERSAO}")
        self.minsize(640, 420)
        self.resizable(True, True)

        self.tema = tk.StringVar(self, value="escuro")
        self.nome_teste = tk.StringVar(self)
        self.data_teste = date.today()
        self.data_texto = tk.StringVar(self, value=f"{self.data_teste:%d/%m/%Y}")
        self.status_teste = tk.StringVar(self, value="Pronto para iniciar")
        self.em_teste = False
        self.perfil_carregado = None
        self.perfil_texto = tk.StringVar(self, value="Perfil na interface: nenhum")
        self.tempo_texto = tk.StringVar(self, value="00:00.0")
        self.feedback = tk.StringVar(self, value="Pronto.")
        self._inicio_cronometro = None
        self._tempo_decorrido = 0.0
        self._cronometro_after = None
        self._confirmacao_motor = None
        self._resumo_compacto = None
        self._calendario = None
        self._layout_compacto = None
        self._cabecalho_baixo = None
        self._margem_atual = None
        self._ajuste_pendente = None

        familias = set(font.families(self))
        self.fonte = next(
            (nome for nome in ("Aptos", "Segoe UI", "DejaVu Sans") if nome in familias),
            font.nametofont("TkDefaultFont").actual("family"),
        )

        self.estilo = ttk.Style(self)
        self.estilo.theme_use("clam")
        # Mantém o comportamento de seleção dos radiobuttons com visual de botão.
        self.estilo.layout("Tema.TRadiobutton", self.estilo.layout("TButton"))

        self.banco_memoria = MemoriaDemonstracao.carregar_exemplos()
        self.banco_perfis = BancoPerfis(caminho_perfis)
        self._criar_tela()
        self.aplicar_tema()
        self._posicionar_janela()
        self.bind("<Configure>", self._ao_redimensionar)
        self.protocol("WM_DELETE_WINDOW", self._fechar)
        self.deiconify()
        self._agendar_layout()

    def _criar_tela(self):
        """Cabeçalho, painel esquerdo e espaço livre à direita."""
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)

        self.conteudo = ttk.Frame(self, style="Fundo.TFrame", padding=28)
        self.conteudo.grid(row=0, column=0, sticky="nsew")
        self.conteudo.columnconfigure(0, weight=1)
        self.conteudo.rowconfigure(1, weight=1)

        self.cabecalho = ttk.Frame(self.conteudo, style="Fundo.TFrame")
        self.cabecalho.grid(row=0, column=0, sticky="ew")
        self.cabecalho.columnconfigure(0, weight=1)

        self.bloco_titulo = ttk.Frame(self.cabecalho, style="Fundo.TFrame")
        self.bloco_titulo.grid(row=0, column=0, sticky="w")
        self.titulo = ttk.Label(
            self.bloco_titulo,
            text="Bancada de empuxo",
            style="Titulo.TLabel",
        )
        self.titulo.grid(row=0, column=0, sticky="w")
        self.subtitulo = ttk.Label(
            self.bloco_titulo,
            text="Sistema de ensaios · Demonstração",
            style="Secundario.TLabel",
        )
        self.subtitulo.grid(row=1, column=0, sticky="w", pady=(5, 0))

        self.seletor_tema = ttk.Frame(self.cabecalho, style="Fundo.TFrame")
        self.seletor_tema.grid(row=0, column=1, sticky="e", padx=(28, 0))
        ttk.Label(
            self.seletor_tema, text="Tema", style="Secundario.TLabel"
        ).grid(row=0, column=0, padx=(0, 12))

        self.botoes_tema = {}
        for coluna, (valor, texto) in enumerate(
            (("escuro", "Escuro"), ("claro", "Claro")), start=1
        ):
            botao = ttk.Radiobutton(
                self.seletor_tema,
                text=texto,
                value=valor,
                variable=self.tema,
                command=self.aplicar_tema,
                style="Tema.TRadiobutton",
                width=7,
                takefocus=True,
                cursor="hand2",
            )
            botao.grid(row=0, column=coluna, padx=(0, 6 if coluna == 1 else 0))
            self.botoes_tema[valor] = botao

        self.abas = ttk.Notebook(self.conteudo, style="Bancada.TNotebook")
        self.abas.grid(row=1, column=0, sticky="nsew", pady=(18, 0))
        self.corpo = ttk.Frame(self.abas, style="Fundo.TFrame", padding=(0, 12, 0, 0))
        self.abas.add(self.corpo, text="Ensaio")
        self.corpo.columnconfigure(1, weight=1)
        self.corpo.rowconfigure(0, weight=1)
        self.painel_esquerdo = PainelRolavel(self.corpo)
        self.painel_esquerdo.grid(row=0, column=0, sticky="nsew")
        self._criar_painel_esquerdo()

        # Resumo sempre visível, independente da rolagem dos comandos à esquerda.
        self.area_principal = tk.Frame(
            self.corpo, borderwidth=0, highlightthickness=1
        )
        self.area_principal.grid(row=0, column=1, sticky="nsew", padx=(20, 0))
        self.area_principal.columnconfigure(0, weight=1)
        self.area_principal.rowconfigure(2, weight=1)
        self._criar_resumo()
        self.area_principal.bind("<Configure>", lambda evento: self._agendar_layout())
        self.aba_memoria = AbaMemoria(self.abas, self, self.banco_memoria)
        self.abas.add(self.aba_memoria, text="Memória de testes")
        self.aba_calibracao = AbaCalibracao(self.abas, self, self.banco_perfis)
        self.abas.add(self.aba_calibracao, text="Calibração")

    def _criar_resumo(self):
        self.resumo = ttk.Frame(self.area_principal, style="Painel.TFrame", padding=16)
        self.resumo.grid(row=0, column=0, sticky="ew")
        self.resumo.columnconfigure(0, weight=1)
        self.bloco_cronometro = ttk.Frame(self.resumo, style="Painel.TFrame")
        self.bloco_cronometro.grid(row=0, column=0, sticky="nw")
        ttk.Label(
            self.bloco_cronometro, text="Tempo do teste", style="Discreto.TLabel"
        ).grid(sticky="w")
        ttk.Label(
            self.bloco_cronometro, textvariable=self.tempo_texto, style="Cronometro.TLabel"
        ).grid(row=1, column=0, sticky="w", pady=(4, 0))

        self.bloco_conexoes = ttk.Frame(self.resumo, style="Painel.TFrame")
        self.bloco_conexoes.grid(row=0, column=1, sticky="ne", padx=(24, 0))
        ttk.Label(
            self.bloco_conexoes, text="Conexões", style="Discreto.TLabel"
        ).grid(row=0, column=0, sticky="w", pady=(0, 4))
        self.indicadores_conexao = {}
        for linha, canal in enumerate(("USB", "LoRa"), 1):
            indicador = ttk.Label(
                self.bloco_conexoes, text=f"● {canal}: desconectado",
                style="Desconectado.TLabel",
            )
            indicador.grid(row=linha, column=0, sticky="w", pady=2)
            self.indicadores_conexao[canal] = indicador

        ttk.Separator(self.area_principal, style="Painel.TSeparator").grid(
            row=1, column=0, sticky="ew", padx=16
        )
        # Área vazia, sem gerar uma curva fictícia. O gráfico completo virá depois.
        self.canvas_grafico = tk.Canvas(
            self.area_principal, width=1, height=1, bd=0, highlightthickness=0
        )
        self.canvas_grafico.grid(row=2, column=0, sticky="nsew", padx=16, pady=12)
        self._aviso_grafico = self.canvas_grafico.create_text(
            0, 0, text="Área do gráfico\nNenhum dado para exibir",
            font=(self.fonte, 10), justify="center", tags=("estado_vazio",),
        )
        self.canvas_grafico.bind("<Configure>", self._posicionar_aviso_grafico)
        self.rotulo_feedback = ttk.Label(
            self.area_principal, textvariable=self.feedback, style="Discreto.TLabel",
            wraplength=300, justify="left",
        )
        self.rotulo_feedback.grid(row=3, column=0, sticky="ew", padx=16, pady=(0, 14))

    def _posicionar_aviso_grafico(self, evento=None):
        largura = self.canvas_grafico.winfo_width()
        altura = self.canvas_grafico.winfo_height()
        self.canvas_grafico.coords(self._aviso_grafico, largura / 2, altura / 2)
        self.canvas_grafico.itemconfigure(self._aviso_grafico, width=max(40, largura - 16))

    def atualizar_indicador_conexao(self, canal, estado):
        """Somente apresentação; futuramente chamada pela integração, na thread da UI."""
        estilos = {
            "desconectado": "Desconectado.TLabel",
            "conectado": "Conectado.TLabel",
            "erro": "ErroConexao.TLabel",
        }
        if canal not in self.indicadores_conexao or estado not in estilos:
            raise ValueError("Canal ou estado de conexão desconhecido")
        self.indicadores_conexao[canal].configure(text=f"● {canal}: {estado}", style=estilos[estado])

    def _criar_painel_esquerdo(self):
        painel = self.painel_esquerdo.conteudo
        ttk.Label(painel, text="Dados do teste", style="Secao.TLabel").grid(
            row=0, column=0, sticky="w"
        )
        ttk.Label(painel, textvariable=self.status_teste, style="Discreto.TLabel").grid(
            row=1, column=0, sticky="w", pady=(4, 16)
        )

        ttk.Label(painel, text="Nome do teste", style="Campo.TLabel").grid(
            row=2, column=0, sticky="w", pady=(0, 6)
        )
        self.entrada_nome = ttk.Entry(
            painel, textvariable=self.nome_teste, style="Campo.TEntry", width=1
        )
        self.entrada_nome.grid(row=3, column=0, sticky="ew")

        ttk.Label(painel, text="Data do teste", style="Campo.TLabel").grid(
            row=4, column=0, sticky="w", pady=(12, 6)
        )
        linha_data = ttk.Frame(painel, style="Painel.TFrame")
        linha_data.grid(row=5, column=0, sticky="ew")
        linha_data.columnconfigure(0, weight=1)
        self.campo_data = ttk.Entry(
            linha_data, textvariable=self.data_texto, width=11,
            state="readonly", style="Campo.TEntry", takefocus=False,
        )
        self.campo_data.grid(row=0, column=0, sticky="ew", padx=(0, 8))
        self.campo_data.bind("<Button-1>", lambda evento: self.abrir_calendario())
        self.botao_data = ttk.Button(
            linha_data, text="Calendário", style="Acao.TButton",
            command=self.abrir_calendario, compound="left",
        )
        self.botao_data.grid(row=0, column=1, sticky="ew")

        ttk.Label(painel, text="Observações", style="Campo.TLabel").grid(
            row=6, column=0, sticky="w", pady=(12, 6)
        )
        linha_obs = ttk.Frame(painel, style="Painel.TFrame")
        linha_obs.grid(row=7, column=0, sticky="ew")
        linha_obs.columnconfigure(0, weight=1)
        self.observacoes = tk.Text(
            linha_obs, height=3, width=1, wrap="word", undo=True,
            font=(self.fonte, 10), padx=8, pady=8, bd=0, highlightthickness=1,
        )
        self.observacoes.grid(row=0, column=0, sticky="ew")
        barra_obs = ttk.Scrollbar(
            linha_obs, orient="vertical", command=self.observacoes.yview,
            style="Painel.Vertical.TScrollbar",
        )
        barra_obs.grid(row=0, column=1, sticky="ns")
        self.observacoes.configure(yscrollcommand=barra_obs.set)
        self.observacoes.bind("<Tab>", lambda evento: self._mudar_foco(self.observacoes, 1))
        self.observacoes.bind("<Shift-Tab>", lambda evento: self._mudar_foco(self.observacoes, -1))
        self.observacoes.bind("<ISO_Left_Tab>", lambda evento: self._mudar_foco(self.observacoes, -1))

        ttk.Separator(painel, style="Painel.TSeparator").grid(
            row=8, column=0, sticky="ew", pady=16
        )
        self.botao_calibrar = ttk.Button(
            painel, text="Calibração", style="Acao.TButton",
            command=lambda: self.abrir_calibracao(editor=True),
        )
        self.botao_calibrar.grid(row=9, column=0, sticky="ew", pady=(0, 8))
        self.botao_perfis = ttk.Button(
            painel, text="Perfis de calibração", style="Acao.TButton",
            command=self.abrir_calibracao,
        )
        self.botao_perfis.grid(row=10, column=0, sticky="ew", pady=(0, 6))
        self.rotulo_perfil = ttk.Label(
            painel, textvariable=self.perfil_texto, style="Discreto.TLabel",
            wraplength=265, justify="left",
        )
        self.rotulo_perfil.grid(row=11, column=0, sticky="ew", pady=(0, 12))
        self.botao_tara = ttk.Button(
            painel, text="Tara / zerar leitura", style="Acao.TButton",
            command=lambda: self._mostrar_previa("Tara / zerar leitura"),
        )
        self.botao_tara.grid(row=12, column=0, sticky="ew", pady=(0, 8))
        linha_execucao = ttk.Frame(painel, style="Painel.TFrame")
        linha_execucao.grid(row=13, column=0, sticky="ew")
        linha_execucao.columnconfigure((0, 1), weight=1, uniform="execucao")
        self.botao_iniciar = ttk.Button(
            linha_execucao, text="Iniciar teste", style="Primario.TButton",
            command=self._iniciar_teste,
        )
        self.botao_iniciar.grid(row=0, column=0, sticky="ew", padx=(0, 4))
        self.botao_parar = ttk.Button(
            linha_execucao, text="Parar teste", style="Acao.TButton",
            command=self._parar_teste, state="disabled",
        )
        self.botao_parar.grid(row=0, column=1, sticky="ew", padx=(4, 0))
        self.moldura_motor = tk.Frame(painel, bd=0, padx=2, pady=2)
        self.moldura_motor.grid(row=14, column=0, sticky="ew", pady=(12, 0))
        self.botao_motor = ttk.Button(
            self.moldura_motor, text="Iniciar motor", style="Motor.TButton",
            command=self._solicitar_inicio_motor,
        )
        self.botao_motor.pack(fill="x")

        ttk.Separator(painel, style="Painel.TSeparator").grid(
            row=15, column=0, sticky="ew", pady=16
        )
        self.botao_limpar = ttk.Button(
            painel, text="Limpar gráfico", style="Acao.TButton", command=self._limpar_grafico
        )
        self.botao_limpar.grid(row=16, column=0, sticky="ew", pady=(0, 8))
        self.botao_salvar = ttk.Button(
            painel, text="Salvar teste", style="Acao.TButton",
            command=lambda: self._mostrar_previa("Salvar teste"),
        )
        self.botao_salvar.grid(row=17, column=0, sticky="ew", pady=(0, 8))
        self.botao_lora = ttk.Button(
            painel, text="Transmitir por LoRa", style="Acao.TButton",
            command=lambda: self._mostrar_previa("Transmitir por LoRa"),
        )
        self.botao_lora.grid(row=18, column=0, sticky="ew")
        ttk.Separator(painel, style="Painel.TSeparator").grid(
            row=19, column=0, sticky="ew", pady=16
        )
        self.botao_memoria = ttk.Button(
            painel, text="Memória de testes", style="Acao.TButton",
            command=self.abrir_memoria,
        )
        self.botao_memoria.grid(row=20, column=0, sticky="ew")
        self.painel_esquerdo.vincular_controles()

    def abrir_calibracao(self, editor=False):
        self.abas.select(self.aba_calibracao)
        if editor:
            self.aba_calibracao.abrir_editor()
        else:
            self.aba_calibracao.voltar_aos_perfis()

    def carregar_perfil(self, perfil):
        """Seleciona os valores para a interface, sem enviar comandos ao ESP32."""
        if self.em_teste:
            return False
        self.perfil_carregado = perfil
        self.perfil_texto.set(f"Perfil na interface: {perfil.nome}")
        return True

    def perfil_foi_salvo(self, perfil):
        if self.perfil_carregado and self.perfil_carregado.identificador == perfil.identificador:
            self.perfil_carregado = None
            self.perfil_texto.set("Perfil alterado: carregue novamente")

    def abrir_memoria(self):
        self.abas.select(self.aba_memoria)
        self.aba_memoria.mostrar_lista()

    @staticmethod
    def _mudar_foco(widget, direcao):
        destino = widget.tk_focusNext() if direcao > 0 else widget.tk_focusPrev()
        destino.focus_set()
        return "break"

    def abrir_calendario(self):
        if self._calendario is not None and self._calendario.winfo_exists():
            self._calendario.lift()
            return
        self._calendario = Calendario(
            self, self.data_teste, self._selecionar_data, self.botao_data
        )

    def _selecionar_data(self, valor):
        self.data_teste = valor
        self.data_texto.set(f"{valor:%d/%m/%Y}")

    def _iniciar_teste(self):
        """Muda somente o estado visual; não inicia aquisição no ESP32."""
        if self.em_teste:
            return
        self.em_teste = True
        self._tempo_decorrido = 0.0
        self._inicio_cronometro = perf_counter()
        self.tempo_texto.set("00:00.0")
        self.status_teste.set("Teste em andamento")
        self.botao_iniciar.state(["disabled"])
        self.botao_parar.state(["!disabled"])
        self.feedback.set("Teste iniciado em demonstração.")
        self._atualizar_cronometro()
        self.aba_calibracao.atualizar_acoes()

    def _parar_teste(self):
        if not self.em_teste:
            return
        self._tempo_decorrido = perf_counter() - self._inicio_cronometro
        self.em_teste = False
        self._inicio_cronometro = None
        if self._cronometro_after is not None:
            self.after_cancel(self._cronometro_after)
            self._cronometro_after = None
        self.tempo_texto.set(formatar_tempo(self._tempo_decorrido))
        self.status_teste.set("Teste encerrado")
        self.botao_iniciar.state(["!disabled"])
        self.botao_parar.state(["disabled"])
        self.feedback.set("Teste encerrado. Tempo final mantido no cronômetro.")
        self.aba_calibracao.atualizar_acoes()

    def _atualizar_cronometro(self):
        self._cronometro_after = None
        if not self.em_teste:
            return
        self._tempo_decorrido = perf_counter() - self._inicio_cronometro
        self.tempo_texto.set(formatar_tempo(self._tempo_decorrido))
        self._cronometro_after = self.after(100, self._atualizar_cronometro)

    def _limpar_grafico(self):
        """Limpa apenas os itens visuais, sem alterar nome, data, notas ou tempo."""
        self.canvas_grafico.delete("dados")
        self.canvas_grafico.itemconfigure(self._aviso_grafico, state="normal")
        self.feedback.set("Visualização do gráfico limpa.")

    def _solicitar_inicio_motor(self):
        if self._confirmacao_motor is not None and self._confirmacao_motor.winfo_exists():
            self._confirmacao_motor.lift()
            return
        self._confirmacao_motor = ConfirmacaoMotor(self, self._confirmar_inicio_motor)

    def _confirmar_inicio_motor(self):
        """Conclusão da confirmação visual; não contém comando de ignição."""
        self.feedback.set("Início do motor confirmado em demonstração. Nenhum comando enviado.")

    def _mostrar_previa(self, acao):
        """Resposta demonstrativa: não salva, transmite, calibra ou aciona motor."""
        aviso = tk.Toplevel(self)
        aviso.withdraw()
        aviso.title(acao)
        aviso.transient(self)
        aviso.resizable(False, False)
        aviso.configure(background=self.cores["superficie"])
        corpo = ttk.Frame(aviso, style="Painel.TFrame", padding=22)
        corpo.grid(sticky="nsew")
        ttk.Label(corpo, text=acao, style="Secao.TLabel").grid(sticky="w")
        texto = (
            "Prévia da interface. Este botão ainda não executa a ação.\n\n"
            "Nenhum comando é enviado à bancada."
        )
        ttk.Label(
            corpo, text=texto, style="Campo.TLabel", wraplength=330, justify="left"
        ).grid(row=1, column=0, sticky="w", pady=(12, 18))
        botao = ttk.Button(corpo, text="Entendi", style="Primario.TButton", command=aviso.destroy)
        botao.grid(row=2, column=0, sticky="e")
        aviso.bind("<Escape>", lambda evento: aviso.destroy())
        aviso.update_idletasks()
        x = max(0, self.winfo_rootx() + (self.winfo_width() - aviso.winfo_reqwidth()) // 2)
        y = max(0, self.winfo_rooty() + (self.winfo_height() - aviso.winfo_reqheight()) // 2)
        aviso.geometry(f"+{x}+{y}")
        aviso.deiconify()
        aviso.wait_visibility()
        aviso.grab_set()
        botao.focus_set()

    def aplicar_tema(self):
        """Atualiza as cores sem recriar a janela ou seus componentes."""
        cores = TEMAS[self.tema.get()]
        self.cores = cores
        self.configure(background=cores["fundo"])
        self.estilo.configure("Fundo.TFrame", background=cores["fundo"])
        self.estilo.configure(
            "Titulo.TLabel",
            background=cores["fundo"],
            foreground=cores["texto"],
            font=(self.fonte, 22, "bold"),
        )
        self.estilo.configure(
            "Secundario.TLabel",
            background=cores["fundo"],
            foreground=cores["secundario"],
            font=(self.fonte, 10),
        )
        self.estilo.configure(
            "TituloCompacto.TLabel", background=cores["fundo"], foreground=cores["texto"],
            font=(self.fonte, 18, "bold"),
        )
        self.estilo.configure(
            "Tema.TRadiobutton",
            font=(self.fonte, 10),
            anchor="center",
            padding=(10, 8),
            borderwidth=1,
            relief="flat",
            background=cores["botao"],
            foreground=cores["texto"],
            bordercolor=cores["borda"],
            lightcolor=cores["borda"],
            darkcolor=cores["borda"],
            focuscolor=cores["foco"],
        )
        self.estilo.map(
            "Tema.TRadiobutton",
            background=[
                ("selected", "active", cores["destaque_hover"]),
                ("selected", cores["destaque"]),
                ("active", cores["hover"]),
            ],
            foreground=[("selected", "#FFFFFF")],
            bordercolor=[
                ("focus", cores["foco"]),
                ("selected", cores["destaque"]),
                ("active", cores["secundario"]),
            ],
            lightcolor=[("selected", cores["destaque"])],
            darkcolor=[("selected", cores["destaque"])],
        )
        self.area_principal.configure(
            background=cores["superficie"],
            highlightbackground=cores["borda"],
            highlightcolor=cores["borda"],
        )
        self._estilizar_painel(cores)
        self.canvas_grafico.configure(background=cores["superficie"])
        self.canvas_grafico.itemconfigure(self._aviso_grafico, fill=cores["secundario"])
        self.estilo.configure(
            "Bancada.TNotebook", background=cores["fundo"], borderwidth=0,
            bordercolor=cores["borda"], lightcolor=cores["borda"], darkcolor=cores["borda"],
        )
        self.estilo.configure(
            "Bancada.TNotebook.Tab", background=cores["botao"], foreground=cores["secundario"],
            font=(self.fonte, 10), padding=(16, 9), borderwidth=0,
            bordercolor=cores["borda"], lightcolor=cores["borda"], darkcolor=cores["borda"],
        )
        self.estilo.map(
            "Bancada.TNotebook.Tab",
            background=[("selected", cores["superficie"]), ("active", cores["hover"])],
            foreground=[("selected", cores["texto"])],
        )
        self.aba_memoria.aplicar_tema(cores)
        self.aba_calibracao.aplicar_tema(cores)

    def _estilizar_painel(self, cores):
        estilo = self.estilo
        estilo.configure("Painel.TFrame", background=cores["superficie"])
        for nome, tamanho, cor, peso in (
            ("Secao.TLabel", 13, "texto", "bold"),
            ("Campo.TLabel", 10, "texto", "normal"),
            ("Discreto.TLabel", 9, "secundario", "normal"),
        ):
            estilo.configure(
                nome, background=cores["superficie"], foreground=cores[cor],
                font=(self.fonte, tamanho, peso),
            )
        estilo.configure(
            "Campo.TEntry", fieldbackground=cores["campo"], foreground=cores["texto"],
            insertcolor=cores["texto"], bordercolor=cores["borda"],
            lightcolor=cores["borda"], darkcolor=cores["borda"],
            padding=(8, 8), font=(self.fonte, 10),
            selectbackground=cores["destaque"], selectforeground="#FFFFFF",
        )
        estilo.map(
            "Campo.TEntry", fieldbackground=[("readonly", cores["campo"])],
            foreground=[("readonly", cores["texto"])],
            bordercolor=[("focus", cores["foco"])],
        )
        # Entry utiliza a opção font do próprio widget, além do estilo.
        self.entrada_nome.configure(font=(self.fonte, 10))
        self.campo_data.configure(font=(self.fonte, 10))
        for nome in ("Acao.TButton", "Primario.TButton", "Motor.TButton", "Dia.TButton", "DiaSelecionado.TButton", "DiaHoje.TButton"):
            primario = nome in ("Primario.TButton", "DiaSelecionado.TButton")
            motor = nome == "Motor.TButton"
            fundo = cores["destaque"] if primario else cores["botao"]
            texto = "#FFFFFF" if primario else cores["motor"] if motor else cores["texto"]
            hover = cores["destaque_hover"] if primario else cores["motor_hover"] if motor else cores["hover"]
            borda = cores["motor"] if motor else cores["destaque"] if primario else cores["borda"]
            estilo.configure(
                nome, font=(self.fonte, 10), padding=(8, 8), relief="flat", borderwidth=1,
                background=fundo, foreground=texto, bordercolor=borda,
                lightcolor=borda, darkcolor=borda, focuscolor=cores["foco"], anchor="center",
            )
            estilo.map(
                nome,
                background=[("disabled", cores["superficie"]), ("active", hover)],
                foreground=[("disabled", cores["desativado"])],
                bordercolor=[("disabled", cores["borda"]), ("focus", cores["foco"])],
                lightcolor=[("disabled", cores["borda"])],
                darkcolor=[("disabled", cores["borda"])],
            )
        for nome in ("Dia.TButton", "DiaSelecionado.TButton", "DiaHoje.TButton"):
            estilo.configure(nome, padding=(2, 5))
        estilo.configure("DiaHoje.TButton", font=(self.fonte, 10, "underline"))
        estilo.configure("Painel.TSeparator", background=cores["borda"])
        estilo.configure(
            "Painel.Vertical.TScrollbar", troughcolor=cores["superficie"],
            background=cores["borda"], bordercolor=cores["superficie"],
            arrowcolor=cores["secundario"], lightcolor=cores["borda"],
            darkcolor=cores["borda"], arrowsize=12,
        )
        estilo.map("Painel.Vertical.TScrollbar", background=[("active", cores["secundario"])])
        self.painel_esquerdo.aplicar_tema(cores)
        self.observacoes.configure(
            background=cores["campo"], foreground=cores["texto"], insertbackground=cores["texto"],
            selectbackground=cores["destaque"], selectforeground="#FFFFFF",
            highlightbackground=cores["borda"], highlightcolor=cores["foco"],
        )
        self._icone_calendario = self._desenhar_icone_calendario(cores["texto"])
        self.botao_data.configure(image=self._icone_calendario)
        self.moldura_motor.configure(background=cores["motor_borda"])
        estilo.configure("Motor.TButton", background=cores["motor_fundo"], font=(self.fonte, 10, "bold"))
        estilo.configure("Cronometro.TLabel", background=cores["superficie"], foreground=cores["texto"], font=(self.fonte, 24, "bold"))
        estilo.configure("TituloMotor.TLabel", background=cores["superficie"], foreground=cores["motor"], font=(self.fonte, 15, "bold"))
        for nome, cor in (("Desconectado.TLabel", "secundario"), ("Conectado.TLabel", "conectado"), ("ErroConexao.TLabel", "motor")):
            estilo.configure(nome, background=cores["superficie"], foreground=cores[cor], font=(self.fonte, 9))
        estilo.configure(
            "Confirmacao.TCheckbutton", background=cores["superficie"], foreground=cores["texto"],
            font=(self.fonte, 10), padding=4, focuscolor=cores["foco"],
            indicatorbackground=cores["campo"], indicatorforeground=cores["texto"],
        )
        estilo.map(
            "Confirmacao.TCheckbutton", background=[("active", cores["superficie"])],
            indicatorbackground=[("selected", cores["destaque"])],
            indicatorforeground=[("selected", "#FFFFFF")],
        )
        estilo.configure(
            "ConfirmarMotor.TButton", font=(self.fonte, 10, "bold"), padding=(10, 9),
            background=cores["confirmar_motor"], foreground="#FFFFFF", borderwidth=1,
            bordercolor=cores["confirmar_motor"], lightcolor=cores["confirmar_motor"],
            darkcolor=cores["confirmar_motor"], focuscolor=cores["foco"],
        )
        estilo.map(
            "ConfirmarMotor.TButton",
            background=[("disabled", cores["botao"]), ("active", cores["confirmar_motor_hover"])],
            foreground=[("disabled", cores["desativado"])],
            bordercolor=[("disabled", cores["borda"]), ("focus", cores["foco"])],
            lightcolor=[("disabled", cores["borda"])], darkcolor=[("disabled", cores["borda"])],
        )

    def _desenhar_icone_calendario(self, cor):
        """Ícone simples, sem arquivos externos ou dependência de emoji."""
        imagem = tk.PhotoImage(master=self, width=22, height=18)
        for retangulo in ((2, 4, 17, 6), (2, 15, 17, 17), (2, 4, 4, 17), (15, 4, 17, 17), (5, 1, 7, 6), (12, 1, 14, 6), (3, 8, 16, 9), (6, 11, 8, 13), (11, 11, 13, 13)):
            imagem.put(cor, to=retangulo)
        return imagem

    def _posicionar_janela(self):
        """Escolhe um tamanho inicial que deixa espaço nas telas de notebook."""
        largura_tela = self.winfo_screenwidth()
        altura_tela = self.winfo_screenheight()
        largura = max(640, min(1100, largura_tela - 80))
        altura = max(420, min(700, altura_tela - 120))
        x = max(0, (largura_tela - largura) // 2)
        y = max(0, (altura_tela - altura) // 2 - 20)
        self.geometry(f"{largura}x{altura}+{x}+{y}")

    def _ao_redimensionar(self, evento):
        if evento.widget is self:
            self._agendar_layout()

    def _agendar_layout(self):
        if self._ajuste_pendente is None:
            self._ajuste_pendente = self.after_idle(self._ajustar_layout)

    def _ajustar_layout(self):
        """Reorganiza o cabeçalho; o grid expande o restante da janela."""
        self._ajuste_pendente = None
        largura = self.winfo_width()
        margem = 16 if largura < 800 else 28
        if margem != self._margem_atual:
            self.conteudo.configure(padding=margem)
            self._margem_atual = margem

        baixo = self.winfo_height() < 540
        if baixo != self._cabecalho_baixo:
            self._cabecalho_baixo = baixo
            self.titulo.configure(style="TituloCompacto.TLabel" if baixo else "Titulo.TLabel")
            self.abas.grid_configure(pady=(10 if baixo else 18, 0))
            if baixo:
                self.subtitulo.grid_remove()
            else:
                self.subtitulo.grid()

        largura_necessaria = (
            self.bloco_titulo.winfo_reqwidth()
            + self.seletor_tema.winfo_reqwidth()
            + 28
        )
        compacto = largura - 2 * margem < largura_necessaria
        if compacto != self._layout_compacto:
            self._layout_compacto = compacto
            if compacto:
                self.seletor_tema.grid_configure(
                    row=1, column=0, sticky="w", padx=0, pady=(18, 0)
                )
            else:
                self.seletor_tema.grid_configure(
                    row=0, column=1, sticky="e", padx=(28, 0), pady=0
                )
        disponivel = self.area_principal.winfo_width() - 34
        necessario = self.bloco_cronometro.winfo_reqwidth() + self.bloco_conexoes.winfo_reqwidth() + 24
        empilhar = disponivel < necessario
        if empilhar != self._resumo_compacto:
            self._resumo_compacto = empilhar
            if empilhar:
                self.bloco_conexoes.grid_configure(row=1, column=0, sticky="w", padx=0, pady=(12, 0))
            else:
                self.bloco_conexoes.grid_configure(row=0, column=1, sticky="ne", padx=(24, 0), pady=0)
        self.rotulo_feedback.configure(wraplength=max(80, disponivel))

    def _fechar(self):
        self.aba_calibracao.confirmar_saida(self._encerrar)

    def _encerrar(self):
        if self._cronometro_after is not None:
            self.after_cancel(self._cronometro_after)
        if self._ajuste_pendente is not None:
            self.after_cancel(self._ajuste_pendente)
        self.destroy()


if __name__ == "__main__":
    BancadaApp().mainloop()
