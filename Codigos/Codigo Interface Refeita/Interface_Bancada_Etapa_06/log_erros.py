"""Log local de erros e avisos. Não consulta nem simula falhas de hardware."""

from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from threading import RLock
from uuid import uuid4
import tkinter as tk
from tkinter import filedialog, ttk

from perfis_calibracao import ErroPerfis, caminho_padrao, gravar_atomico, mesmos_arquivos


def caminho_log_padrao(pasta=None):
    pasta = Path(pasta) if pasta is not None else caminho_padrao().parent
    nome = datetime.now().strftime("erros_%Y%m%d_%H%M%S_") + uuid4().hex[:6] + ".log"
    return pasta / "logs" / nome


def texto_seguro(valor):
    return str(valor).encode("utf-8", errors="replace").decode("utf-8")


@dataclass(frozen=True)
class Registro:
    horario: str
    nivel: str
    origem: str
    mensagem: str
    detalhes: str

    def texto(self):
        corpo = f"[{self.horario}] {self.nivel} | {self.origem}\n{self.mensagem}\n"
        if self.detalhes:
            corpo += self.detalhes.rstrip() + "\n"
        return corpo + "\n"


class RegistroErros:
    """A escrita pode vir de outra thread; a janela é atualizada só pelo Tk."""

    def __init__(self, caminho=None):
        self.caminho = Path(caminho) if caminho is not None else caminho_log_padrao()
        self._registros = []
        self._lock = RLock()
        self.falha_gravacao = None

    def registrar(self, origem, mensagem, detalhes="", nivel="ERRO"):
        if nivel not in ("ERRO", "AVISO"):
            raise ValueError("O nível do log deve ser ERRO ou AVISO.")
        registro = Registro(datetime.now().astimezone().isoformat(timespec="seconds"), nivel,
                            texto_seguro(origem), texto_seguro(mensagem), texto_seguro(detalhes))
        with self._lock:
            self._registros.append(registro)
            if self.falha_gravacao is None:
                try:
                    self.caminho.parent.mkdir(parents=True, exist_ok=True)
                    with self.caminho.open("a", encoding="utf-8") as arquivo:
                        arquivo.write(registro.texto())
                except (OSError, UnicodeError) as erro:
                    self.falha_gravacao = texto_seguro(erro)
                    self._registros.append(Registro(
                        registro.horario, "ERRO", "Gravação do log",
                        "Não foi possível gravar o log automático. Os registros continuam disponíveis nesta sessão para copiar ou salvar.",
                        self.falha_gravacao,
                    ))
        return registro

    def listar(self):
        with self._lock:
            return tuple(self._registros)

    def texto(self):
        return "Log de erros — Bancada de empuxo\n\n" + "".join(r.texto() for r in self.listar())


class JanelaLog(tk.Toplevel):
    def __init__(self, app):
        super().__init__(app)
        self.withdraw()
        self.app = app
        self.registro = app.registro_erros
        self.title("Log de erros — Bancada de empuxo")
        self.transient(app)
        self.minsize(520, 340)
        self._exibidos = 0
        self._placeholder = True
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)
        self.corpo = ttk.Frame(self, style="Painel.TFrame", padding=16)
        self.corpo.grid(sticky="nsew")
        self.corpo.columnconfigure(0, weight=1)
        self.corpo.rowconfigure(2, weight=1)
        ttk.Label(self.corpo, text="Log de erros", style="Secao.TLabel").grid(row=0, sticky="w")
        self.rotulo_contexto = ttk.Label(self.corpo,
            text="Erros e avisos desta sessão. Falhas do ESP aparecerão após a integração.",
            style="Discreto.TLabel", wraplength=650, justify="left")
        self.rotulo_contexto.grid(row=1, sticky="ew", pady=(4, 12))
        area = ttk.Frame(self.corpo, style="Painel.TFrame")
        area.grid(row=2, sticky="nsew")
        area.columnconfigure(0, weight=1)
        area.rowconfigure(0, weight=1)
        self.texto = tk.Text(area, wrap="word", width=1, height=1, bd=0,
                             highlightthickness=1, font=(app.fonte, 10), padx=10, pady=10)
        self.texto.grid(row=0, column=0, sticky="nsew")
        barra = ttk.Scrollbar(area, command=self.texto.yview, style="Painel.Vertical.TScrollbar")
        barra.grid(row=0, column=1, sticky="ns")
        self.texto.configure(yscrollcommand=barra.set)
        self.texto.insert("1.0", "Nenhum erro registrado nesta sessão.")
        self.texto.configure(state="disabled")
        acoes = ttk.Frame(self.corpo, style="Painel.TFrame")
        acoes.grid(row=3, sticky="ew", pady=(12, 0))
        acoes.columnconfigure((0, 1, 2), weight=1, uniform="log_acoes")
        self.botao_copiar = ttk.Button(acoes, text="Copiar log", style="Acao.TButton", command=self.copiar)
        self.botao_copiar.grid(row=0, column=0, sticky="ew", padx=(0, 4))
        self.botao_salvar = ttk.Button(acoes, text="Salvar .txt", style="Primario.TButton", command=self.salvar)
        self.botao_salvar.grid(row=0, column=1, sticky="ew", padx=4)
        self.botao_fechar = ttk.Button(acoes, text="Fechar", style="Acao.TButton", command=self.destroy)
        self.botao_fechar.grid(row=0, column=2, sticky="ew", padx=(4, 0))
        self.mensagem = tk.StringVar(self, value="O log é salvo automaticamente quando ocorre um erro ou aviso.")
        self.rotulo_mensagem = ttk.Label(self.corpo, textvariable=self.mensagem,
            style="Discreto.TLabel", wraplength=650, justify="left")
        self.rotulo_mensagem.grid(row=4, sticky="ew", pady=(8, 0))
        self.corpo.bind("<Configure>", self._ajustar)
        self.bind("<Escape>", lambda evento: self.destroy())
        largura = min(800, max(520, app.winfo_screenwidth() - 60))
        altura = min(500, max(340, app.winfo_screenheight() - 100))
        x = max(0, min(app.winfo_rootx() + (app.winfo_width() - largura) // 2,
                       app.winfo_screenwidth() - largura))
        y = max(0, min(app.winfo_rooty() + (app.winfo_height() - altura) // 2,
                       app.winfo_screenheight() - altura - 40))
        self.geometry(f"{largura}x{altura}+{x}+{y}")
        self.aplicar_tema(app.cores)
        self.atualizar()
        self.deiconify()
        self.texto.focus_set()

    def _ajustar(self, evento):
        for rotulo in (self.rotulo_contexto, self.rotulo_mensagem):
            rotulo.configure(wraplength=max(200, evento.width - 32))

    def atualizar(self):
        registros = self.registro.listar()
        if len(registros) != self._exibidos:
            acompanhar = self.texto.yview()[1] >= 0.98
            self.texto.configure(state="normal")
            if self._placeholder:
                self.texto.delete("1.0", "end")
                self._placeholder = False
            for registro in registros[self._exibidos:]:
                self.texto.insert("end", f"[{registro.horario}] {registro.nivel} | {registro.origem}\n", registro.nivel)
                self.texto.insert("end", registro.mensagem + "\n")
                if registro.detalhes:
                    self.texto.insert("end", registro.detalhes.rstrip() + "\n", "detalhes")
                self.texto.insert("end", "\n")
            self.texto.configure(state="disabled")
            self._exibidos = len(registros)
            if acompanhar:
                self.texto.see("end")
            if self.registro.falha_gravacao:
                self.mensagem.set("A gravação automática falhou. Copie ou salve os registros desta sessão.")
        estado = ["!disabled"] if registros else ["disabled"]
        self.botao_copiar.state(estado)
        self.botao_salvar.state(estado)

    def copiar(self):
        self.clipboard_clear()
        self.clipboard_append(self.registro.texto())
        self.mensagem.set("Log copiado. Você pode colá-lo para analisar ou compartilhar.")

    def salvar(self):
        caminho = filedialog.asksaveasfilename(parent=self, title="Salvar log de erros",
            defaultextension=".txt", initialfile="log_erros_bancada.txt", filetypes=(("Arquivo de texto", "*.txt"),))
        if not caminho:
            return
        try:
            destino = Path(caminho)
            if destino.suffix.lower() != ".txt":
                raise ValueError("Use a extensão .txt para salvar uma cópia do log.")
            if mesmos_arquivos(destino, self.app.banco_perfis.caminho) or mesmos_arquivos(destino, self.registro.caminho):
                raise ValueError("Escolha outro arquivo para salvar a cópia do log.")
            gravar_atomico(destino, self.registro.texto().encode("utf-8"))
        except (ValueError, ErroPerfis) as erro:
            self.app.registrar_erro("Exportação do log", str(erro))
            self.atualizar()
            self.mensagem.set(str(erro))
            return
        self.mensagem.set(f"Log salvo em {destino.name} na pasta escolhida.")

    def aplicar_tema(self, cores):
        self.configure(background=cores["superficie"])
        self.texto.configure(background=cores["campo"], foreground=cores["texto"],
            insertbackground=cores["texto"], selectbackground=cores["destaque"],
            selectforeground="#FFFFFF", highlightbackground=cores["borda"], highlightcolor=cores["foco"])
        self.texto.tag_configure("ERRO", foreground=cores["motor"], font=(self.app.fonte, 10, "bold"))
        self.texto.tag_configure("AVISO", foreground=cores["curva"], font=(self.app.fonte, 10, "bold"))
        self.texto.tag_configure("detalhes", foreground=cores["secundario"])
