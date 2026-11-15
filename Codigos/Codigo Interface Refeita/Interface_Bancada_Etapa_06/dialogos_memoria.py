"""Diálogos da memória de demonstração, sem comandos de hardware."""

import tkinter as tk
from tkinter import ttk


class DialogoMemoria(tk.Toplevel):
    def __init__(self, parent, titulo, mensagem, testes, confirmar=None, ao_confirmar=None):
        super().__init__(parent)
        self.withdraw()
        self.title(titulo)
        self.transient(parent)
        self.resizable(False, False)
        self.configure(background=parent.cores["superficie"])
        self.finalizado = False
        self.ao_confirmar = ao_confirmar
        corpo = ttk.Frame(self, style="Painel.TFrame", padding=22)
        corpo.grid(sticky="nsew")
        corpo.columnconfigure(0, weight=1)
        ttk.Label(corpo, text=titulo, style="Secao.TLabel").grid(sticky="w")
        ttk.Label(corpo, text=mensagem, wraplength=380, justify="left",
                  style="Campo.TLabel").grid(row=1, sticky="w", pady=(12, 12))
        nomes = [f"• {teste.nome} — {teste.data:%d/%m/%Y}" for teste in testes[:5]]
        if len(testes) > 5:
            nomes.append(f"… e mais {len(testes) - 5} testes.")
        ttk.Label(corpo, text="\n".join(nomes), wraplength=380, justify="left",
                  style="Discreto.TLabel").grid(row=2, sticky="w", pady=(0, 18))
        acoes = ttk.Frame(corpo, style="Painel.TFrame")
        acoes.grid(row=3, sticky="ew")
        acoes.columnconfigure(1, weight=1)
        self.botao_cancelar = ttk.Button(
            acoes, text="Cancelar" if confirmar else "Fechar", style="Acao.TButton",
            command=self.cancelar,
        )
        self.botao_cancelar.grid(row=0, column=0, sticky="w")
        self.botao_confirmar = None
        if confirmar:
            self.botao_confirmar = ttk.Button(
                acoes, text=confirmar, style="ExcluirMemoria.TButton", command=self.confirmar,
            )
            self.botao_confirmar.grid(row=0, column=1, sticky="e", padx=(24, 0))
        self.protocol("WM_DELETE_WINDOW", self.cancelar)
        self.bind("<Escape>", lambda evento: self.cancelar())
        self.update_idletasks()
        x = parent.winfo_rootx() + (parent.winfo_width() - self.winfo_reqwidth()) // 2
        y = parent.winfo_rooty() + (parent.winfo_height() - self.winfo_reqheight()) // 2
        x = max(0, min(x, self.winfo_screenwidth() - self.winfo_reqwidth()))
        y = max(0, min(y, self.winfo_screenheight() - self.winfo_reqheight() - 40))
        self.geometry(f"+{x}+{y}")
        self.deiconify()
        self.wait_visibility()
        self.grab_set()
        self.botao_cancelar.focus_set()

    def confirmar(self):
        if self.finalizado or self.ao_confirmar is None:
            return
        self.finalizado = True
        self.grab_release()
        self.destroy()
        self.ao_confirmar()

    def cancelar(self):
        if self.finalizado:
            return
        self.finalizado = True
        self.grab_release()
        self.destroy()
