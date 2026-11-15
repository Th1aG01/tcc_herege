"""Componentes visuais compartilhados da bancada."""

import tkinter as tk
from tkinter import ttk


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


