"""Cadastro de perfis de calibração com gravação local e seleção na interface."""

from datetime import datetime
from pathlib import Path
import re
import tkinter as tk
from tkinter import filedialog, ttk

from componentes import PainelRolavel
from perfis_calibracao import ErroPerfis


def texto_numero(valor):
    return "" if valor is None else str(valor).replace(".", ",")


class ConfirmarDescarte(tk.Toplevel):
    def __init__(self, app, ao_descartar, *, titulo="Alterações não salvas",
                 mensagem="Este perfil tem alterações não salvas. Deseja descartá-las?",
                 cancelar="Continuar editando", confirmar="Descartar"):
        super().__init__(app)
        self.withdraw()
        self.title(titulo)
        self.transient(app)
        self.resizable(False, False)
        self.finalizado = False
        self.ao_descartar = ao_descartar
        self.configure(background=app.cores["superficie"])
        corpo = ttk.Frame(self, style="Painel.TFrame", padding=22)
        corpo.grid(sticky="nsew")
        ttk.Label(corpo, text=titulo, style="Secao.TLabel").grid(sticky="w")
        ttk.Label(corpo, text=mensagem,
                  wraplength=360, justify="left", style="Campo.TLabel").grid(row=1, sticky="w", pady=(12, 18))
        acoes = ttk.Frame(corpo, style="Painel.TFrame")
        acoes.grid(row=2, sticky="ew")
        acoes.columnconfigure(1, weight=1)
        self.botao_cancelar = ttk.Button(acoes, text=cancelar, style="Acao.TButton", command=self.cancelar)
        self.botao_cancelar.grid(row=0, column=0, sticky="w")
        self.botao_descartar = ttk.Button(acoes, text=confirmar, style="ExcluirMemoria.TButton", command=self.descartar)
        self.botao_descartar.grid(row=0, column=1, sticky="e", padx=(16, 0))
        self.protocol("WM_DELETE_WINDOW", self.cancelar)
        self.bind("<Escape>", lambda evento: self.cancelar())
        self.update_idletasks()
        x = app.winfo_rootx() + (app.winfo_width() - self.winfo_reqwidth()) // 2
        y = app.winfo_rooty() + (app.winfo_height() - self.winfo_reqheight()) // 2
        x = max(0, min(x, self.winfo_screenwidth() - self.winfo_reqwidth()))
        y = max(0, min(y, self.winfo_screenheight() - self.winfo_reqheight() - 40))
        self.geometry(f"+{x}+{y}")
        self.deiconify()
        self.wait_visibility()
        self.grab_set()
        self.botao_cancelar.focus_set()

    def cancelar(self):
        if self.finalizado:
            return
        self.finalizado = True
        self.grab_release()
        self.destroy()

    def descartar(self):
        if self.finalizado:
            return
        self.cancelar()
        self.ao_descartar()


class AbaCalibracao(ttk.Frame):
    def __init__(self, parent, app, banco):
        super().__init__(parent, style="Fundo.TFrame", padding=(0, 12, 0, 0))
        self.app, self.banco = app, banco
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)
        self.identificador_editado = None
        self.form_em_uso = False
        self._original = None
        self.dialogo = None
        self.campos = {nome: tk.StringVar(self) for nome in
                       ("nome", "celula", "coeficiente", "capacidade_kgf")}
        self.mensagem = tk.StringVar(self, value="Crie um perfil ou selecione um dos perfis salvos.")
        self.mensagem_editor = tk.StringVar(self)
        self.resumo = tk.StringVar(self, value="Nenhum perfil selecionado.")
        self._criar_lista()
        self._criar_editor()
        self.atualizar_lista()
        self._mostrar_lista()

    def _criar_lista(self):
        self.lista = ttk.Frame(self, style="Painel.TFrame", padding=16)
        self.lista.columnconfigure(0, weight=1)
        self.lista.rowconfigure(2, weight=1)
        ttk.Label(self.lista, text="Perfis de calibração", style="Secao.TLabel").grid(sticky="w")
        self.subtitulo_lista = ttk.Label(self.lista, text="Configurações salvas neste computador", style="Discreto.TLabel")
        self.subtitulo_lista.grid(row=1, sticky="w", pady=(4, 12))
        self._lista_compacta = None
        area = ttk.Frame(self.lista, style="Painel.TFrame")
        area.grid(row=2, sticky="nsew")
        area.rowconfigure(0, weight=1)
        area.columnconfigure(0, weight=1)
        self.tabela = ttk.Treeview(area, columns=("nome", "capacidade", "data"),
                                   show="headings", selectmode="browse", height=1, style="Memoria.Treeview")
        for coluna, titulo, largura in (("nome", "Perfil / célula", 320), ("capacidade", "Capacidade", 115), ("data", "Salvo em", 110)):
            self.tabela.heading(coluna, text=titulo)
            self.tabela.column(coluna, width=largura, minwidth=90 if coluna != "nome" else 140,
                               stretch=coluna == "nome", anchor="w")
        self.tabela.grid(row=0, column=0, sticky="nsew")
        barra = ttk.Scrollbar(area, orient="vertical", command=self.tabela.yview, style="Painel.Vertical.TScrollbar")
        barra.grid(row=0, column=1, sticky="ns")
        self.tabela.configure(yscrollcommand=barra.set)
        self.tabela.bind("<<TreeviewSelect>>", lambda evento: self.atualizar_acoes())
        self.vazio = ttk.Label(area, text="Nenhum perfil salvo.\nUse Novo perfil ou Importar JSON.", style="Discreto.TLabel", justify="center")
        self.rotulo_resumo = ttk.Label(self.lista, textvariable=self.resumo, style="Discreto.TLabel", wraplength=600, justify="left")
        self.rotulo_resumo.grid(row=3, sticky="ew", pady=(8, 8))
        acoes = ttk.Frame(self.lista, style="Painel.TFrame")
        acoes.grid(row=4, sticky="ew")
        acoes.columnconfigure((0, 1, 2), weight=1, uniform="acoes_perfil")
        self.botao_novo = ttk.Button(acoes, text="Novo perfil", style="Acao.TButton", command=self.novo_perfil)
        self.botao_novo.grid(row=0, column=0, sticky="ew", padx=(0, 4))
        self.botao_editar = ttk.Button(acoes, text="Editar perfil", style="Acao.TButton", command=self.editar_selecionado)
        self.botao_editar.grid(row=0, column=1, sticky="ew", padx=4)
        self.botao_carregar = ttk.Button(acoes, text="Carregar perfil", style="Primario.TButton", command=self.carregar_selecionado)
        self.botao_carregar.grid(row=0, column=2, sticky="ew", padx=(4, 0))
        self.botao_importar = ttk.Button(acoes, text="Importar JSON", style="Acao.TButton", command=self.importar_perfis)
        self.botao_importar.grid(row=1, column=0, sticky="ew", padx=(0, 4), pady=(8, 0))
        self.botao_exportar = ttk.Button(acoes, text="Exportar perfil", style="Acao.TButton", command=self.exportar_selecionado)
        self.botao_exportar.grid(row=1, column=1, sticky="ew", padx=4, pady=(8, 0))
        self.botao_excluir = ttk.Button(acoes, text="Excluir perfil", style="ExcluirMemoria.TButton", command=self.solicitar_exclusao)
        self.botao_excluir.grid(row=1, column=2, sticky="ew", padx=(4, 0), pady=(8, 0))
        self.rotulo_mensagem = ttk.Label(self.lista, textvariable=self.mensagem, style="Discreto.TLabel", wraplength=600, justify="left")
        self.rotulo_mensagem.grid(row=5, sticky="ew", pady=(8, 0))
        self.lista.bind("<Configure>", self._ajustar_lista)

    def _criar_editor(self):
        self.editor = ttk.Frame(self, style="Painel.TFrame", padding=16)
        self.editor.columnconfigure(0, weight=1)
        self.editor.rowconfigure(2, weight=1)
        self.titulo_editor = ttk.Label(self.editor, text="Novo perfil", style="Secao.TLabel")
        self.titulo_editor.grid(sticky="w")
        ttk.Label(self.editor, text="Informe os valores da calibração. Campos com * são obrigatórios.", style="Discreto.TLabel").grid(row=1, sticky="w", pady=(4, 10))
        self.formulario = PainelRolavel(self.editor, largura=1)
        self.formulario.grid(row=2, sticky="nsew")
        corpo = self.formulario.conteudo
        corpo.columnconfigure((0, 1), weight=1, uniform="campos_perfil")
        self.entradas = {}
        descricoes = (
            ("nome", "Nome do perfil *"), ("celula", "Identificação da célula"),
            ("coeficiente", "Coeficiente de calibração *"), ("capacidade_kgf", "Capacidade nominal (kgf)"),
        )
        for indice, (chave, rotulo) in enumerate(descricoes):
            linha, coluna = divmod(indice, 2)
            campo = ttk.Frame(corpo, style="Painel.TFrame")
            campo.grid(row=linha, column=coluna, sticky="ew", padx=(0, 8) if coluna == 0 else (8, 0), pady=(0, 14))
            campo.columnconfigure(0, weight=1)
            ttk.Label(campo, text=rotulo, style="Campo.TLabel").grid(sticky="w", pady=(0, 6))
            entrada = ttk.Entry(campo, textvariable=self.campos[chave], style="Campo.TEntry", width=1, font=(self.app.fonte, 10))
            entrada.grid(row=1, sticky="ew")
            self.entradas[chave] = entrada
        ttk.Label(corpo, text="Observações", style="Campo.TLabel").grid(row=2, column=0, columnspan=2, sticky="w", pady=(0, 6))
        self.observacoes = tk.Text(corpo, width=1, height=3, wrap="word", undo=True, bd=0, highlightthickness=1,
                                   font=(self.app.fonte, 10), padx=8, pady=8)
        self.observacoes.grid(row=3, column=0, columnspan=2, sticky="ew")
        self.observacoes.bind("<Tab>", lambda evento: self.app._mudar_foco(self.observacoes, 1))
        self.observacoes.bind("<Shift-Tab>", lambda evento: self.app._mudar_foco(self.observacoes, -1))
        ttk.Label(corpo, text="Unidade padrão: kgf. Informe o coeficiente calibrado em kgf. A capacidade é opcional.",
                  style="Discreto.TLabel", wraplength=470, justify="left").grid(row=4, column=0, columnspan=2, sticky="w", pady=(10, 0))
        self.formulario.vincular_controles()
        acoes = ttk.Frame(self.editor, style="Painel.TFrame")
        acoes.grid(row=3, sticky="ew", pady=(12, 0))
        acoes.columnconfigure(1, weight=1)
        self.botao_voltar = ttk.Button(acoes, text="Voltar aos perfis", style="Acao.TButton", command=self.voltar_aos_perfis)
        self.botao_voltar.grid(row=0, column=0, sticky="w")
        self.botao_salvar = ttk.Button(acoes, text="Salvar perfil", style="Primario.TButton", command=self.salvar)
        self.botao_salvar.grid(row=0, column=1, sticky="e")
        self.rotulo_erro = ttk.Label(self.editor, textvariable=self.mensagem_editor, style="Discreto.TLabel", wraplength=600, justify="left")
        self.rotulo_erro.grid(row=4, sticky="ew", pady=(8, 0))
        self.editor.bind("<Configure>", lambda evento: self.rotulo_erro.configure(wraplength=max(200, evento.width - 32)))

    def _ajustar_lista(self, evento):
        compacta = evento.height < 360
        if compacta != self._lista_compacta:
            self._lista_compacta = compacta
            self.lista.configure(padding=10 if compacta else 16)
            if compacta:
                self.subtitulo_lista.grid_remove()
            else:
                self.subtitulo_lista.grid()
            self.rotulo_resumo.grid_configure(pady=(6, 6) if compacta else (8, 8))
            self.rotulo_mensagem.grid_configure(pady=(6, 0) if compacta else (8, 0))
        for rotulo in (self.rotulo_resumo, self.rotulo_mensagem):
            rotulo.configure(wraplength=max(200, evento.width - (20 if compacta else 32)))

    def atualizar_lista(self, selecionar=None):
        itens = self.tabela.get_children()
        if itens:
            self.tabela.delete(*itens)
        for perfil in self.banco.listar():
            capacidade = f"{texto_numero(perfil.capacidade_kgf)} kgf" if perfil.capacidade_kgf is not None else "—"
            self.tabela.insert("", "end", iid=perfil.identificador,
                               values=(perfil.nome, capacidade, datetime.fromisoformat(perfil.salvo_em).strftime("%d/%m/%Y")))
        if selecionar:
            self.tabela.selection_set(selecionar)
            self.tabela.focus(selecionar)
            self.tabela.see(selecionar)
        if not self.banco.listar():
            self.vazio.place(relx=0.5, rely=0.5, y=16, anchor="center")
        else:
            self.vazio.place_forget()
        if self.banco.erro_leitura:
            self.vazio.configure(text="Os perfis não puderam ser lidos.\nO arquivo existente foi preservado.")
            self.mensagem.set(self.banco.erro_leitura)
        self.atualizar_acoes()

    def selecionado(self):
        selecao = self.tabela.selection()
        return self.banco.obter(selecao[0]) if selecao else None

    def atualizar_acoes(self):
        perfil = self.selecionado()
        disponivel = not self.banco.erro_leitura
        self.botao_novo.state(["!disabled"] if disponivel else ["disabled"])
        self.botao_editar.state(["!disabled"] if perfil and disponivel else ["disabled"])
        self.botao_carregar.state(["!disabled"] if perfil and disponivel and not self.app.em_teste else ["disabled"])
        self.botao_importar.state(["!disabled"] if disponivel else ["disabled"])
        self.botao_exportar.state(["!disabled"] if perfil and disponivel else ["disabled"])
        pode_excluir = perfil and disponivel and not self._exclusao_bloqueada(perfil.identificador)
        self.botao_excluir.state(["!disabled"] if pode_excluir else ["disabled"])
        if self.app.em_teste:
            self.mensagem.set("Ensaio em andamento. Pare o teste para carregar outro perfil.")
        elif self.mensagem.get() in ("Ensaio em andamento. Pare o teste para carregar outro perfil.", "Pare o teste antes de carregar outro perfil.", "Pare o teste antes de excluir o perfil carregado."):
            self.mensagem.set("Selecione um perfil para editar ou carregar na interface.")
        if perfil:
            self.resumo.set(f"Coeficiente: {texto_numero(perfil.coeficiente)}  ·  Empuxo em kgf")
        else:
            self.resumo.set("Nenhum perfil selecionado.")

    def _conteudo(self):
        # Offset de arquivos antigos é preservado como metadado, sem controle na tela.
        antigo = self.banco.obter(self.identificador_editado) if self.identificador_editado else None
        return {**{chave: var.get() for chave, var in self.campos.items()}, "unidade": "kgf",
                "offset": antigo.offset if antigo else None, "observacoes": self.observacoes.get("1.0", "end-1c")}

    def tem_alteracoes(self):
        return self.form_em_uso and self._conteudo() != self._original

    def confirmar_saida(self, acao):
        if not self.tem_alteracoes():
            acao()
        elif self.dialogo is not None and self.dialogo.winfo_exists():
            self.dialogo.lift()
        else:
            self.dialogo = ConfirmarDescarte(self.app, acao)

    def _mostrar_lista(self):
        self.editor.grid_remove()
        self.lista.grid(row=0, column=0, sticky="nsew")

    def voltar_aos_perfis(self):
        def voltar():
            self.form_em_uso = False
            self._mostrar_lista()
        self.confirmar_saida(voltar)

    def abrir_editor(self):
        if self.form_em_uso:
            self.lista.grid_remove()
            self.editor.grid(row=0, column=0, sticky="nsew")
        else:
            self.novo_perfil()

    def novo_perfil(self):
        if not self.banco.erro_leitura:
            self.confirmar_saida(lambda: self._preencher_editor(None))

    def editar_selecionado(self):
        perfil = self.selecionado()
        if perfil:
            self.confirmar_saida(lambda: self._preencher_editor(perfil))

    def _preencher_editor(self, perfil):
        self.identificador_editado = perfil.identificador if perfil else None
        for chave, var in self.campos.items():
            valor = getattr(perfil, chave) if perfil else ""
            var.set(texto_numero(valor) if chave in ("capacidade_kgf", "coeficiente") else valor)
        self.observacoes.delete("1.0", "end")
        self.observacoes.insert("1.0", perfil.observacoes if perfil else "")
        self.observacoes.edit_reset()
        self._original = self._conteudo()
        self.form_em_uso = True
        self.titulo_editor.configure(text="Editar perfil" if perfil else "Novo perfil")
        self.botao_salvar.configure(text="Salvar alterações" if perfil else "Salvar perfil")
        self.mensagem_editor.set("Os valores serão salvos no computador. Nenhum comando será enviado ao ESP32.")
        self.rotulo_erro.configure(style="Discreto.TLabel")
        self.lista.grid_remove()
        self.editor.grid(row=0, column=0, sticky="nsew")
        self.formulario.canvas.yview_moveto(0)
        self.entradas["nome"].focus_set()

    def salvar(self):
        if not self.form_em_uso:
            return
        carregado = self.app.perfil_carregado
        if self.app.em_teste and carregado and carregado.identificador == self.identificador_editado:
            self._erro("Pare o teste antes de alterar o perfil carregado.")
            return
        try:
            perfil = self.banco.salvar(self._conteudo(), self.identificador_editado)
        except (ValueError, ErroPerfis) as erro:
            self._erro(str(erro), nivel="ERRO" if isinstance(erro, ErroPerfis) else "AVISO")
            return
        self.app.perfil_foi_salvo(perfil)
        self._original = self._conteudo()
        self.form_em_uso = False
        self.atualizar_lista(perfil.identificador)
        self._mostrar_lista()
        self.mensagem.set(f"Perfil “{perfil.nome}” salvo. Use Carregar perfil para selecioná-lo na interface.")

    def _erro(self, mensagem, nivel="AVISO"):
        self.app.registrar_erro("Calibração", mensagem, nivel=nivel)
        self.mensagem_editor.set(mensagem)
        self.rotulo_erro.configure(style="ErroConexao.TLabel")

    def carregar_selecionado(self):
        perfil = self.selecionado()
        if not perfil:
            return
        if not self.app.carregar_perfil(perfil):
            self.mensagem.set("Pare o teste antes de carregar outro perfil.")
            return
        self.mensagem.set(f"Perfil “{perfil.nome}” carregado na interface. Nenhum valor enviado ao ESP32.")

    def importar_perfis(self):
        if self.banco.erro_leitura:
            return
        caminho = filedialog.askopenfilename(
            parent=self.app, title="Importar perfil de calibração",
            filetypes=(("Arquivos JSON", "*.json"),),
        )
        if not caminho:
            return
        try:
            importados, renomeados = self.banco.importar(caminho)
        except (ValueError, ErroPerfis) as erro:
            self.app.registrar_erro("Perfis de calibração", str(erro), nivel="ERRO" if isinstance(erro, ErroPerfis) else "AVISO")
            self.mensagem.set(str(erro))
            return
        self.atualizar_lista(importados[0].identificador)
        quantidade = len(importados)
        texto = f"{quantidade} {'perfil importado e salvo' if quantidade == 1 else 'perfis importados e salvos'} no computador."
        if renomeados:
            texto += f" {renomeados} {'nome ajustado' if renomeados == 1 else 'nomes ajustados'} para preservar os existentes."
        convertidos = self.banco.ultima_importacao_convertidos
        if convertidos:
            aviso = f"{convertidos} coeficiente(s) anterior(es) convertido(s) para kgf."
            texto += " " + aviso
            self.app.registrar_erro("Importação de perfis", aviso, nivel="AVISO")
        self.mensagem.set(texto)

    def exportar_selecionado(self):
        perfil = self.selecionado()
        if perfil is None:
            return
        nome = re.sub(r'[<>:"/\\|?*\x00-\x1f]', '_', perfil.nome).strip(' .') or 'calibracao'
        caminho = filedialog.asksaveasfilename(
            parent=self.app, title="Exportar perfil de calibração", defaultextension=".json",
            initialfile=f"perfil_{nome}.json", filetypes=(("Arquivos JSON", "*.json"),),
        )
        if not caminho:
            return
        try:
            destino = self.banco.exportar(perfil.identificador, caminho)
        except (ValueError, ErroPerfis) as erro:
            self.app.registrar_erro("Perfis de calibração", str(erro), nivel="ERRO" if isinstance(erro, ErroPerfis) else "AVISO")
            self.mensagem.set(str(erro))
            return
        self.mensagem.set(f"Perfil exportado para {Path(destino).name} na pasta escolhida.")

    def _exclusao_bloqueada(self, identificador):
        carregado = self.app.perfil_carregado
        return bool(self.app.em_teste and carregado and carregado.identificador == identificador)

    def solicitar_exclusao(self):
        perfil = self.selecionado()
        if perfil is None:
            return
        if self._exclusao_bloqueada(perfil.identificador):
            self.mensagem.set("Pare o teste antes de excluir o perfil carregado.")
            return
        if self.dialogo is not None and self.dialogo.winfo_exists():
            self.dialogo.lift()
            return
        texto = f"Excluir o perfil “{perfil.nome}” deste computador?\n\nEssa ação remove a configuração salva."
        if self.app.perfil_carregado and self.app.perfil_carregado.identificador == perfil.identificador:
            texto += " O perfil também deixará de estar carregado na interface."
        self.dialogo = ConfirmarDescarte(
            self.app, lambda: self._excluir_confirmado(perfil.identificador),
            titulo="Excluir perfil?", mensagem=texto, cancelar="Cancelar", confirmar="Excluir perfil",
        )

    def _excluir_confirmado(self, identificador):
        if self._exclusao_bloqueada(identificador):
            self.mensagem.set("Pare o teste antes de excluir o perfil carregado.")
            return
        try:
            removido = self.banco.excluir(identificador)
        except (ValueError, ErroPerfis) as erro:
            self.app.registrar_erro("Perfis de calibração", str(erro), nivel="ERRO" if isinstance(erro, ErroPerfis) else "AVISO")
            self.mensagem.set(str(erro))
            return
        self.app.perfil_foi_excluido(identificador)
        if self.identificador_editado == identificador:
            self.identificador_editado = None
            self.form_em_uso = False
        self.atualizar_lista()
        self.mensagem.set(f"Perfil “{removido.nome}” excluído do computador.")

    def aplicar_tema(self, cores):
        self.formulario.aplicar_tema(cores)
        self.observacoes.configure(
            background=cores["campo"], foreground=cores["texto"], insertbackground=cores["texto"],
            selectbackground=cores["destaque"], selectforeground="#FFFFFF",
            highlightbackground=cores["borda"], highlightcolor=cores["foco"],
        )
