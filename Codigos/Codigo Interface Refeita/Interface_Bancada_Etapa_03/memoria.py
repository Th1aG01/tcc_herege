"""Aba de consulta, seleção e visualização dos testes de demonstração."""

import tkinter as tk
from tkinter import ttk

from componentes import PainelRolavel
from dados_memoria import numero
from dialogos_memoria import DialogoMemoria
from grafico import GraficoHistorico


class AbaMemoria(ttk.Frame):
    def __init__(self, parent, app, banco):
        super().__init__(parent, style="Fundo.TFrame", padding=(0, 12, 0, 0))
        self.app = app
        self.banco = banco
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)
        self.variaveis = {}
        self.checkboxes = {}
        self.todos = tk.BooleanVar(self, value=False)
        self.contagem = tk.StringVar(self)
        self.mensagem = tk.StringVar(self, value="Marque um teste para abrir o gráfico; marque vários para transmitir ou excluir.")
        self.teste_aberto = None
        self.dialogo = None
        self._criar_lista()
        self._criar_detalhe()
        self.reconstruir_lista()
        self.mostrar_lista()

    def _criar_lista(self):
        self.pagina_lista = ttk.Frame(self, style="Painel.TFrame", padding=16)
        self.pagina_lista.grid(row=0, column=0, sticky="nsew")
        self.pagina_lista.columnconfigure(0, weight=1)
        self.pagina_lista.rowconfigure(3, weight=1)
        ttk.Label(self.pagina_lista, text="Memória de testes", style="Secao.TLabel").grid(sticky="w")
        ttk.Label(
            self.pagina_lista, text="Registros de demonstração · alterações somente nesta sessão",
            style="Discreto.TLabel",
        ).grid(row=1, sticky="w", pady=(4, 12))
        selecao = ttk.Frame(self.pagina_lista, style="Painel.TFrame")
        selecao.grid(row=2, sticky="ew", pady=(0, 8))
        selecao.columnconfigure(1, weight=1)
        self.checkbox_todos = ttk.Checkbutton(
            selecao, text="Selecionar todos", variable=self.todos,
            command=self.alternar_todos, style="Confirmacao.TCheckbutton",
        )
        self.checkbox_todos.grid(row=0, column=0, sticky="w")
        ttk.Label(selecao, textvariable=self.contagem, style="Discreto.TLabel").grid(
            row=0, column=1, sticky="e"
        )

        self.lista = PainelRolavel(self.pagina_lista, largura=1)
        self.lista.grid(row=3, sticky="nsew")
        self.lista.conteudo.configure(padding=(10, 6))
        self.acoes = ttk.Frame(self.pagina_lista, style="Painel.TFrame")
        self.acoes.grid(row=4, sticky="ew", pady=(12, 6))
        self.acoes.columnconfigure((0, 1, 2), weight=1, uniform="acao_memoria")
        self.botao_abrir = ttk.Button(self.acoes, text="Abrir gráfico", style="Primario.TButton", command=self.abrir_marcado)
        self.botao_abrir.grid(row=0, column=0, sticky="ew", padx=(0, 4))
        self.botao_transmitir = ttk.Button(self.acoes, text="Transmitir por LoRa", style="Acao.TButton", command=self.transmitir_marcados)
        self.botao_transmitir.grid(row=0, column=1, sticky="ew", padx=4)
        self.botao_excluir = ttk.Button(self.acoes, text="Excluir selecionados", style="ExcluirMemoria.TButton", command=self.solicitar_exclusao)
        self.botao_excluir.grid(row=0, column=2, sticky="ew", padx=(4, 0))
        self.feedback = ttk.Label(self.pagina_lista, textvariable=self.mensagem, style="Discreto.TLabel", justify="left", wraplength=600)
        self.feedback.grid(row=5, sticky="ew")
        self.pagina_lista.bind("<Configure>", lambda evento: self.feedback.configure(wraplength=max(200, evento.width - 32)))

    def reconstruir_lista(self):
        selecionados = {ident for ident, var in self.variaveis.items() if var.get()}
        for filho in self.lista.conteudo.winfo_children():
            filho.destroy()
        self.variaveis.clear()
        self.checkboxes.clear()
        corpo = self.lista.conteudo
        corpo.columnconfigure(0, weight=1)
        corpo.columnconfigure(1, minsize=105)
        corpo.columnconfigure(2, minsize=85)
        for coluna, texto in enumerate(("Teste", "Data", "Duração")):
            ttk.Label(corpo, text=texto, style="Discreto.TLabel").grid(row=0, column=coluna, sticky="w", padx=4, pady=(0, 8))
        testes = self.banco.listar()
        for linha, teste in enumerate(testes, 1):
            var = tk.BooleanVar(self, value=teste.identificador in selecionados)
            self.variaveis[teste.identificador] = var
            check = ttk.Checkbutton(
                corpo, text=teste.nome, variable=var,
                command=self.atualizar_selecao, style="Confirmacao.TCheckbutton",
            )
            check.grid(row=linha, column=0, sticky="ew", padx=(0, 12), pady=10)
            self.checkboxes[teste.identificador] = check
            ttk.Label(corpo, text=f"{teste.data:%d/%m/%Y}", style="Campo.TLabel").grid(row=linha, column=1, sticky="w", padx=4)
            ttk.Label(corpo, text=f"{numero(teste.duracao)} s", style="Campo.TLabel").grid(row=linha, column=2, sticky="w", padx=4)
        if not testes:
            ttk.Label(
                corpo, text="Nenhum teste na memória de demonstração.\nOs exemplos voltam ao reiniciar a interface.",
                style="Discreto.TLabel", justify="left", wraplength=450,
            ).grid(row=1, column=0, columnspan=3, sticky="w", pady=18)
        self.lista.vincular_controles()
        self.lista.canvas.yview_moveto(0)
        self.atualizar_selecao()

    def selecionados(self):
        return [teste for teste in self.banco.listar() if self.variaveis[teste.identificador].get()]

    def atualizar_selecao(self):
        quantidade = len(self.selecionados())
        total = len(self.variaveis)
        self.contagem.set(f"{quantidade} de {total} selecionados")
        self.todos.set(total > 0 and quantidade == total)
        self.checkbox_todos.state(["!disabled"] if total else ["disabled"])
        self.checkbox_todos.state(["alternate"] if 0 < quantidade < total else ["!alternate"])
        self.botao_abrir.state(["!disabled"] if quantidade == 1 else ["disabled"])
        for botao in (self.botao_transmitir, self.botao_excluir):
            botao.state(["!disabled"] if quantidade else ["disabled"])
        if quantidade > 1:
            self.mensagem.set("Para abrir um gráfico, mantenha apenas um teste marcado. LoRa e exclusão aceitam vários.")
        else:
            self.mensagem.set("Marque um teste para abrir o gráfico; marque vários para transmitir ou excluir.")

    def alternar_todos(self):
        marcar = not (self.variaveis and all(var.get() for var in self.variaveis.values()))
        for var in self.variaveis.values():
            var.set(marcar)
        self.atualizar_selecao()

    def abrir_marcado(self):
        testes = self.selecionados()
        if len(testes) == 1:
            self.exibir_teste(testes[0])

    def solicitar_exclusao(self):
        testes = self.selecionados()
        if not testes or self._dialogo_aberto():
            return
        ids = tuple(teste.identificador for teste in testes)
        quantidade = len(ids)
        substantivo = "teste" if quantidade == 1 else "testes"
        self.dialogo = DialogoMemoria(
            self.app, f"Excluir {quantidade} {substantivo}?",
            "Os registros abaixo serão removidos desta sessão de demonstração.",
            testes, confirmar=f"Excluir {quantidade} {substantivo}",
            ao_confirmar=lambda: self._excluir_confirmados(ids),
        )

    def _excluir_confirmados(self, ids):
        quantidade = self.banco.excluir(ids)
        if self.teste_aberto and self.teste_aberto.identificador in ids:
            self.teste_aberto = None
            self.mostrar_lista()
        self.reconstruir_lista()
        self.mensagem.set(f"{quantidade} {'teste removido' if quantidade == 1 else 'testes removidos'} da demonstração.")

    def _dialogo_aberto(self):
        if self.dialogo is not None and self.dialogo.winfo_exists():
            self.dialogo.lift()
            return True
        return False

    def transmitir_marcados(self):
        self.mostrar_transmissao(self.selecionados())

    def mostrar_transmissao(self, testes):
        if not testes or self._dialogo_aberto():
            return
        quantidade = len(testes)
        amostras = sum(len(teste.amostras) for teste in testes)
        self.dialogo = DialogoMemoria(
            self.app, "Transmissão por LoRa",
            f"Prévia: {quantidade} {'teste' if quantidade == 1 else 'testes'}, {amostras} amostras.\n"
            "A transmissão ainda não está integrada. Nenhum dado foi enviado.", testes,
        )

    def _criar_detalhe(self):
        self.pagina_detalhe = ttk.Frame(self, style="Painel.TFrame", padding=(16, 12))
        self.pagina_detalhe.columnconfigure(0, weight=1)
        self.pagina_detalhe.rowconfigure(3, weight=1)
        topo = ttk.Frame(self.pagina_detalhe, style="Painel.TFrame")
        topo.grid(row=0, sticky="ew", pady=(0, 8))
        topo.columnconfigure(1, weight=1)
        self.botao_voltar = ttk.Button(topo, text="Voltar à lista", style="Acao.TButton", padding=(8, 6), command=self.mostrar_lista)
        self.botao_voltar.grid(row=0, column=0, sticky="w")
        self.botao_transmitir_aberto = ttk.Button(
            topo, text="Transmitir por LoRa", style="Acao.TButton", padding=(8, 6),
            command=lambda: self.mostrar_transmissao([self.teste_aberto]) if self.teste_aberto else None,
        )
        self.botao_transmitir_aberto.grid(row=0, column=1, sticky="e")
        self.titulo_detalhe = ttk.Label(self.pagina_detalhe, style="Secao.TLabel")
        self.titulo_detalhe.grid(row=1, sticky="w")
        self.metadados = ttk.Label(self.pagina_detalhe, style="Discreto.TLabel", justify="left", wraplength=600)
        self.metadados.grid(row=2, sticky="ew", pady=(4, 10))
        self.detalhes = ttk.Notebook(self.pagina_detalhe, style="Bancada.TNotebook")
        self.detalhes.grid(row=3, sticky="nsew")
        self.grafico = GraficoHistorico(self.detalhes, self.app.fonte)
        self.detalhes.add(self.grafico, text="Gráfico")
        pagina_amostras = ttk.Frame(self.detalhes, style="Painel.TFrame")
        pagina_amostras.rowconfigure(0, weight=1)
        pagina_amostras.columnconfigure(0, weight=1)
        self.tabela = ttk.Treeview(
            pagina_amostras, columns=("tempo", "empuxo"), show="headings",
            style="Memoria.Treeview", height=1, selectmode="browse",
        )
        self.tabela.heading("tempo", text="Tempo (s)")
        self.tabela.heading("empuxo", text="Empuxo (N)")
        self.tabela.column("tempo", width=150, minwidth=90, anchor="center")
        self.tabela.column("empuxo", width=150, minwidth=90, anchor="center")
        self.tabela.grid(row=0, column=0, sticky="nsew")
        barra = ttk.Scrollbar(pagina_amostras, orient="vertical", command=self.tabela.yview, style="Painel.Vertical.TScrollbar")
        barra.grid(row=0, column=1, sticky="ns")
        self.tabela.configure(yscrollcommand=barra.set)
        self.detalhes.add(pagina_amostras, text="Amostras")
        pagina_obs = ttk.Frame(self.detalhes, style="Painel.TFrame")
        pagina_obs.rowconfigure(0, weight=1)
        pagina_obs.columnconfigure(0, weight=1)
        self.observacoes = tk.Text(
            pagina_obs, width=1, height=1, wrap="word", bd=0, padx=16, pady=16,
            font=(self.app.fonte, 10), state="disabled",
        )
        self.observacoes.grid(row=0, column=0, sticky="nsew")
        rolagem_obs = ttk.Scrollbar(pagina_obs, orient="vertical", command=self.observacoes.yview, style="Painel.Vertical.TScrollbar")
        rolagem_obs.grid(row=0, column=1, sticky="ns")
        self.observacoes.configure(yscrollcommand=rolagem_obs.set)
        self.detalhes.add(pagina_obs, text="Observações")
        self.pagina_detalhe.bind("<Configure>", self._ajustar_detalhe)

    def _ajustar_detalhe(self, evento):
        self.metadados.configure(wraplength=max(220, evento.width - 32))
        self.titulo_detalhe.configure(wraplength=max(220, evento.width - 32))

    def exibir_teste(self, teste):
        self.teste_aberto = teste
        self.titulo_detalhe.configure(text=teste.nome)
        self.metadados.configure(text=(
            f"{teste.data:%d/%m/%Y}  ·  {numero(teste.duracao)} s  ·  {len(teste.amostras)} amostras  ·  Demonstração"
        ))
        itens = self.tabela.get_children()
        if itens:
            self.tabela.delete(*itens)
        self.tabela.heading("empuxo", text=f"Empuxo ({teste.unidade})")
        for indice, (tempo, empuxo) in enumerate(teste.amostras):
            self.tabela.insert("", "end", iid=str(indice), values=(numero(tempo, 3), numero(empuxo, 3)))
        self.tabela.yview_moveto(0)
        self.observacoes.configure(state="normal")
        self.observacoes.delete("1.0", "end")
        self.observacoes.insert("1.0", teste.observacoes or "Sem observações.")
        self.observacoes.configure(state="disabled")
        self.pagina_lista.grid_remove()
        self.pagina_detalhe.grid(row=0, column=0, sticky="nsew")
        self.detalhes.select(self.grafico)
        self.grafico.exibir(teste)

    def mostrar_lista(self):
        self.pagina_detalhe.grid_remove()
        self.pagina_lista.grid(row=0, column=0, sticky="nsew")

    def aplicar_tema(self, cores):
        self.lista.aplicar_tema(cores)
        self.grafico.aplicar_tema(cores)
        self.observacoes.configure(background=cores["superficie"], foreground=cores["texto"], selectbackground=cores["destaque"], selectforeground="#FFFFFF")
        estilo = self.app.estilo
        estilo.configure(
            "ExcluirMemoria.TButton", background=cores["motor_fundo"], foreground=cores["motor"],
            bordercolor=cores["motor_borda"], lightcolor=cores["motor_borda"], darkcolor=cores["motor_borda"],
            borderwidth=1, padding=(8, 8), font=(self.app.fonte, 10),
        )
        estilo.map(
            "ExcluirMemoria.TButton", background=[("disabled", cores["superficie"]), ("active", cores["motor_hover"])],
            foreground=[("disabled", cores["desativado"])],
        )
        estilo.configure(
            "Memoria.Treeview", background=cores["superficie"], fieldbackground=cores["superficie"],
            foreground=cores["texto"], bordercolor=cores["borda"], rowheight=28, font=(self.app.fonte, 10),
        )
        estilo.configure(
            "Memoria.Treeview.Heading", background=cores["botao"], foreground=cores["texto"],
            relief="flat", font=(self.app.fonte, 10, "bold"), padding=8,
        )
        estilo.map("Memoria.Treeview", background=[("selected", cores["destaque"])], foreground=[("selected", "#FFFFFF")])
        estilo.map("Memoria.Treeview.Heading", background=[("active", cores["hover"])])
