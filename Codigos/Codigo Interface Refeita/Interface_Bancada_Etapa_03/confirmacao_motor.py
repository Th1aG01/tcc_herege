"""Confirmação visual do motor. Este módulo não acessa o hardware."""

import tkinter as tk
from tkinter import ttk


class ConfirmacaoMotor(tk.Toplevel):
    def __init__(self, parent, ao_confirmar):
        super().__init__(parent)
        self.withdraw()
        self.title("Confirmar início do motor")
        self.transient(parent)
        self.resizable(False, False)
        self.configure(background=parent.cores["superficie"])
        self._ao_confirmar = ao_confirmar
        self._finalizado = False
        self.concordo = tk.BooleanVar(self, value=False)

        corpo = ttk.Frame(self, style="Painel.TFrame", padding=24)
        corpo.grid(sticky="nsew")
        corpo.columnconfigure(0, weight=1)
        ttk.Label(corpo, text="Iniciar motor", style="TituloMotor.TLabel").grid(sticky="w")
        ttk.Label(
            corpo, text="Confirme sua intenção antes de prosseguir.",
            style="Campo.TLabel", wraplength=350, justify="left",
        ).grid(row=1, column=0, sticky="w", pady=(12, 16))
        self.checkbox = ttk.Checkbutton(
            corpo, text="Sim, eu quero ligar o motor", variable=self.concordo,
            command=self._atualizar_botao, style="Confirmacao.TCheckbutton",
        )
        self.checkbox.grid(row=2, column=0, sticky="w")
        ttk.Label(
            corpo, text="Modo de demonstração: nenhum comando será enviado.",
            style="Discreto.TLabel", wraplength=350, justify="left",
        ).grid(row=3, column=0, sticky="w", pady=(16, 20))

        acoes = ttk.Frame(corpo, style="Painel.TFrame")
        acoes.grid(row=4, column=0, sticky="ew")
        acoes.columnconfigure(1, weight=1)
        self.botao_cancelar = ttk.Button(
            acoes, text="Cancelar", style="Acao.TButton", command=self.cancelar
        )
        self.botao_cancelar.grid(row=0, column=0, sticky="w", padx=(0, 12))
        self.botao_confirmar = ttk.Button(
            acoes, text="Confirmar início", style="ConfirmarMotor.TButton",
            state="disabled", command=self.confirmar,
        )
        self.botao_confirmar.grid(row=0, column=1, sticky="e")

        self.protocol("WM_DELETE_WINDOW", self.cancelar)
        self.bind("<Escape>", lambda evento: self.cancelar())
        # Nenhum atalho global de Enter: a confirmação exige uma ação explícita.
        self.update_idletasks()
        largura, altura = self.winfo_reqwidth(), self.winfo_reqheight()
        x = parent.winfo_rootx() + (parent.winfo_width() - largura) // 2
        y = parent.winfo_rooty() + (parent.winfo_height() - altura) // 2
        x = max(8, min(x, self.winfo_screenwidth() - largura - 8))
        y = max(8, min(y, self.winfo_screenheight() - altura - 40))
        self.geometry(f"+{x}+{y}")
        self.deiconify()
        self.wait_visibility()
        self.grab_set()
        self.botao_cancelar.focus_set()

    def _atualizar_botao(self):
        self.botao_confirmar.state(["!disabled"] if self.concordo.get() else ["disabled"])

    def confirmar(self):
        # A condição também é conferida no callback, além do botão desabilitado.
        if self._finalizado or not self.concordo.get():
            return
        self._finalizado = True
        self._fechar()
        self._ao_confirmar()

    def cancelar(self):
        if self._finalizado:
            return
        self._finalizado = True
        self._fechar()

    def _fechar(self):
        self.grab_release()
        self.destroy()
        if self.master.winfo_exists():
            self.master.botao_motor.focus_set()
