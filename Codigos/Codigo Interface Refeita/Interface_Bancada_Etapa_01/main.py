"""Bancada de empuxo — etapa 02: identificação e comandos do teste.

Execute com: python main.py
Os próximos componentes serão acrescentados em ``self.area_principal``.
"""

import tkinter as tk
from datetime import date
from tkinter import font, ttk

from calendario import Calendario


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
        "motor": "#E6B56D",
        "motor_hover": "#352A1E",
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
        "motor": "#855000",
        "motor_hover": "#FFF0D8",
        "desativado": "#778393",
    },
}


class PainelRolavel(tk.Frame):
    """Mantém os controles acessíveis em janelas de notebook menores."""

    def __init__(self, parent, largura=330):
        super().__init__(parent, width=largura, height=1, bd=0, highlightthickness=1)
        self.grid_propagate(False)
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)
        self.canvas = tk.Canvas(self, width=1, height=1, bd=0, highlightthickness=0)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        self.barra = ttk.Scrollbar(
            self, orient="vertical", command=self.canvas.yview,
            style="Painel.Vertical.TScrollbar",
        )
        self.barra.grid(row=0, column=1, sticky="ns")
        self.canvas.configure(yscrollcommand=self.barra.set)
        self.conteudo = ttk.Frame(self.canvas, style="Painel.TFrame", padding=16)
        self.conteudo.columnconfigure(0, weight=1)
        self._janela = self.canvas.create_window((0, 0), window=self.conteudo, anchor="nw")
        self.conteudo.bind("<Configure>", self._atualizar)
        self.canvas.bind("<Configure>", self._redimensionar)

    def _redimensionar(self, evento):
        self.canvas.itemconfigure(self._janela, width=evento.width)
        self._atualizar()

    def _atualizar(self, evento=None):
        limites = self.canvas.bbox("all")
        if not limites:
            return
        self.canvas.configure(scrollregion=limites)
        if limites[3] > self.canvas.winfo_height():
            self.barra.grid()
        else:
            self.barra.grid_remove()
            self.canvas.yview_moveto(0)

    def vincular_controles(self):
        """Roda do mouse apenas neste painel; Tab traz o campo focado à vista."""
        def vincular(widget):
            if not isinstance(widget, tk.Text):
                widget.bind("<MouseWheel>", self._rolar, add="+")
                widget.bind("<Button-4>", self._rolar, add="+")
                widget.bind("<Button-5>", self._rolar, add="+")
            widget.bind("<FocusIn>", self._mostrar_foco, add="+")
            for filho in widget.winfo_children():
                vincular(filho)
        vincular(self)

    def _rolar(self, evento):
        limites = self.canvas.bbox("all")
        if not limites or limites[3] <= self.canvas.winfo_height():
            return
        if getattr(evento, "num", None) == 4:
            passos = -3
        elif getattr(evento, "num", None) == 5:
            passos = 3
        else:
            delta = evento.delta
            if not delta:
                return
            passos = (-1 if delta > 0 else 1) * max(1, abs(delta) // 120) * 3
        self.canvas.yview_scroll(passos, "units")
        return "break"

    def _mostrar_foco(self, evento):
        widget = evento.widget
        if widget in (self, self.canvas, self.barra, self.conteudo):
            return
        altura_total = self.conteudo.winfo_height()
        altura_visivel = self.canvas.winfo_height()
        if altura_total <= altura_visivel:
            return
        topo = widget.winfo_rooty() - self.conteudo.winfo_rooty()
        base = topo + widget.winfo_height()
        inicio = self.canvas.canvasy(0)
        if topo < inicio:
            self.canvas.yview_moveto(max(0, topo - 8) / altura_total)
        elif base > inicio + altura_visivel:
            self.canvas.yview_moveto((base - altura_visivel + 8) / altura_total)

    def aplicar_tema(self, cores):
        self.configure(
            background=cores["superficie"], highlightbackground=cores["borda"],
            highlightcolor=cores["borda"],
        )
        self.canvas.configure(background=cores["superficie"])


class BancadaApp(tk.Tk):
    """Janela única que receberá as funções da bancada, passo a passo."""

    def __init__(self):
        super().__init__()
        self.withdraw()  # Exibe a janela depois de aplicar as cores iniciais.
        self.title("Bancada de empuxo")
        self.minsize(640, 420)
        self.resizable(True, True)

        self.tema = tk.StringVar(self, value="escuro")
        self.nome_teste = tk.StringVar(self)
        self.data_teste = date.today()
        self.data_texto = tk.StringVar(self, value=f"{self.data_teste:%d/%m/%Y}")
        self.status_teste = tk.StringVar(self, value="Pronto para iniciar")
        self.em_teste = False
        self._calendario = None
        self._layout_compacto = None
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
        ttk.Label(
            self.bloco_titulo,
            text="Sistema de ensaios · Demonstração",
            style="Secundario.TLabel",
        ).grid(row=1, column=0, sticky="w", pady=(5, 0))

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

        self.corpo = ttk.Frame(self.conteudo, style="Fundo.TFrame")
        self.corpo.grid(row=1, column=0, sticky="nsew", pady=(24, 0))
        self.corpo.columnconfigure(1, weight=1)
        self.corpo.rowconfigure(0, weight=1)
        self.painel_esquerdo = PainelRolavel(self.corpo)
        self.painel_esquerdo.grid(row=0, column=0, sticky="nsew")
        self._criar_painel_esquerdo()

        # Esta área continua livre para os próximos elementos escolhidos pelo Thiago.
        self.area_principal = tk.Frame(
            self.corpo, borderwidth=0, highlightthickness=1
        )
        self.area_principal.grid(row=0, column=1, sticky="nsew", padx=(20, 0))
        self.area_principal.columnconfigure(0, weight=1)
        self.area_principal.rowconfigure(0, weight=1)

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
            command=lambda: self._mostrar_previa("Calibração"),
        )
        self.botao_calibrar.grid(row=9, column=0, sticky="ew", pady=(0, 8))
        linha_execucao = ttk.Frame(painel, style="Painel.TFrame")
        linha_execucao.grid(row=10, column=0, sticky="ew")
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
        self.botao_motor = ttk.Button(
            painel, text="Iniciar motor", style="Motor.TButton",
            command=lambda: self._mostrar_previa("Iniciar motor"),
        )
        self.botao_motor.grid(row=11, column=0, sticky="ew", pady=(12, 0))

        ttk.Separator(painel, style="Painel.TSeparator").grid(
            row=12, column=0, sticky="ew", pady=16
        )
        self.botao_salvar = ttk.Button(
            painel, text="Salvar teste", style="Acao.TButton",
            command=lambda: self._mostrar_previa("Salvar teste"),
        )
        self.botao_salvar.grid(row=13, column=0, sticky="ew", pady=(0, 8))
        self.botao_lora = ttk.Button(
            painel, text="Transmitir por LoRa", style="Acao.TButton",
            command=lambda: self._mostrar_previa("Transmitir por LoRa"),
        )
        self.botao_lora.grid(row=14, column=0, sticky="ew")
        self.painel_esquerdo.vincular_controles()

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
        self.em_teste = True
        self.status_teste.set("Teste em andamento")
        self.botao_iniciar.state(["disabled"])
        self.botao_parar.state(["!disabled"])

    def _parar_teste(self):
        self.em_teste = False
        self.status_teste.set("Teste encerrado")
        self.botao_iniciar.state(["!disabled"])
        self.botao_parar.state(["disabled"])

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

        largura_necessaria = (
            self.bloco_titulo.winfo_reqwidth()
            + self.seletor_tema.winfo_reqwidth()
            + 28
        )
        compacto = largura - 2 * margem < largura_necessaria
        if compacto == self._layout_compacto:
            return
        self._layout_compacto = compacto
        if compacto:
            self.seletor_tema.grid_configure(
                row=1, column=0, sticky="w", padx=0, pady=(18, 0)
            )
        else:
            self.seletor_tema.grid_configure(
                row=0, column=1, sticky="e", padx=(28, 0), pady=0
            )

    def _fechar(self):
        if self._ajuste_pendente is not None:
            self.after_cancel(self._ajuste_pendente)
        self.destroy()


if __name__ == "__main__":
    BancadaApp().mainloop()
