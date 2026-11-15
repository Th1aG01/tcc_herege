"""Seletor de data em português, feito com Tkinter e a biblioteca padrão."""

import calendar
from datetime import date
import tkinter as tk
from tkinter import ttk


MESES = (
    "Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho",
    "Julho", "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro",
)


class Calendario(tk.Toplevel):
    """Abre próximo ao campo de data e devolve um objeto datetime.date."""

    def __init__(self, parent, selecionada, ao_selecionar, ancora):
        super().__init__(parent)
        self.withdraw()
        self.title("Selecionar data")
        self.transient(parent)
        self.resizable(False, False)
        self.configure(background=parent.cores["superficie"])
        self.selecionada = selecionada
        self.ano, self.mes = selecionada.year, selecionada.month
        self.ao_selecionar = ao_selecionar
        self.ancora = ancora
        self._calendario = calendar.Calendar(firstweekday=calendar.SUNDAY)
        self.botoes_dia = {}

        conteudo = ttk.Frame(self, padding=16, style="Painel.TFrame")
        conteudo.grid(sticky="nsew")
        cabecalho = ttk.Frame(conteudo, style="Painel.TFrame")
        cabecalho.grid(row=0, column=0, sticky="ew", pady=(0, 12))
        cabecalho.columnconfigure(1, weight=1)
        self.anterior = ttk.Button(
            cabecalho, text="‹", width=3, style="Acao.TButton",
            command=lambda: self.mudar_mes(-1),
        )
        self.anterior.grid(row=0, column=0)
        self.rotulo_mes = ttk.Label(cabecalho, style="Secao.TLabel", anchor="center")
        self.rotulo_mes.grid(row=0, column=1, padx=12, sticky="ew")
        self.proximo = ttk.Button(
            cabecalho, text="›", width=3, style="Acao.TButton",
            command=lambda: self.mudar_mes(1),
        )
        self.proximo.grid(row=0, column=2)

        self.grade = ttk.Frame(conteudo, style="Painel.TFrame")
        self.grade.grid(row=1, column=0, sticky="ew")
        for coluna, nome in enumerate(("Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb")):
            self.grade.columnconfigure(coluna, weight=1, uniform="dias")
            ttk.Label(
                self.grade, text=nome, anchor="center", style="Discreto.TLabel"
            ).grid(row=0, column=coluna, sticky="ew", pady=(0, 6))
        for linha in range(1, 7):
            self.grade.rowconfigure(linha, minsize=34)

        ttk.Label(
            conteudo,
            text=f"Selecionada: {selecionada:%d/%m/%Y}",
            style="Discreto.TLabel",
        ).grid(row=2, column=0, sticky="w", pady=(12, 12))
        rodape = ttk.Frame(conteudo, style="Painel.TFrame")
        rodape.grid(row=3, column=0, sticky="ew")
        rodape.columnconfigure(0, weight=1)
        ttk.Button(
            rodape, text="Hoje", style="Acao.TButton",
            command=lambda: self.escolher(date.today()),
        ).grid(row=0, column=0, sticky="w")
        ttk.Button(
            rodape, text="Cancelar", style="Acao.TButton", command=self.fechar
        ).grid(row=0, column=1, sticky="e")

        self.bind("<Escape>", lambda evento: self.fechar())
        self.bind("<Control-Left>", lambda evento: self.mudar_mes(-1))
        self.bind("<Control-Right>", lambda evento: self.mudar_mes(1))
        self.protocol("WM_DELETE_WINDOW", self.fechar)
        self._mostrar_mes()
        self.update_idletasks()

        largura, altura = self.winfo_reqwidth(), self.winfo_reqheight()
        x = min(ancora.winfo_rootx(), self.winfo_screenwidth() - largura - 12)
        y = ancora.winfo_rooty() + ancora.winfo_height() + 6
        if y + altura > self.winfo_screenheight() - 40:
            y = ancora.winfo_rooty() - altura - 6
        y = max(12, min(y, self.winfo_screenheight() - altura - 40))
        self.geometry(f"+{max(12, x)}+{y}")
        self.deiconify()
        self.wait_visibility()
        self.grab_set()
        self.botoes_dia[selecionada.day].focus_set()

    def mudar_mes(self, deslocamento):
        """Navega também na passagem entre dezembro e janeiro."""
        indice = (self.ano - 1) * 12 + self.mes - 1 + deslocamento
        if not 0 <= indice < 9999 * 12:
            return
        ano, mes = divmod(indice, 12)
        self.ano, self.mes = ano + 1, mes + 1
        self._mostrar_mes()

    def _mostrar_mes(self):
        for botao in self.botoes_dia.values():
            botao.destroy()
        self.botoes_dia.clear()
        self.rotulo_mes.configure(text=f"{MESES[self.mes - 1]} {self.ano}")
        self.anterior.state(["disabled"] if (self.ano, self.mes) == (1, 1) else ["!disabled"])
        self.proximo.state(["disabled"] if (self.ano, self.mes) == (9999, 12) else ["!disabled"])
        hoje = date.today()
        for linha, semana in enumerate(self._calendario.monthdayscalendar(self.ano, self.mes), 1):
            for coluna, dia in enumerate(semana):
                if not dia:
                    continue
                valor = date(self.ano, self.mes, dia)
                estilo = "Dia.TButton"
                if valor == self.selecionada:
                    estilo = "DiaSelecionado.TButton"
                elif valor == hoje:
                    estilo = "DiaHoje.TButton"
                botao = ttk.Button(
                    self.grade, text=str(dia), width=3, style=estilo,
                    command=lambda data=valor: self.escolher(data),
                )
                botao.grid(row=linha, column=coluna, padx=2, pady=2, sticky="ew")
                self.botoes_dia[dia] = botao

    def escolher(self, valor):
        self.ao_selecionar(valor)
        self.fechar()

    def fechar(self):
        self.grab_release()
        self.destroy()
        if self.ancora.winfo_exists():
            self.ancora.focus_set()
