"""Registros fictícios para testar a interface, sem acessar Flash, USB ou LoRa."""

from dataclasses import dataclass
from datetime import date
import json
from math import isfinite
from pathlib import Path


@dataclass(frozen=True)
class TesteSalvo:
    identificador: str
    nome: str
    data: date
    observacoes: str
    unidade: str
    amostras: tuple

    @property
    def duracao(self):
        return self.amostras[-1][0] - self.amostras[0][0]

    @property
    def pico(self):
        return max(valor for _, valor in self.amostras)

    @classmethod
    def de_dict(cls, registro):
        amostras = tuple((float(t), float(f)) for t, f in registro["amostras"])
        if not amostras or any(not isfinite(v) for par in amostras for v in par):
            raise ValueError("O registro precisa de amostras numéricas finitas.")
        if amostras[0][0] < 0 or any(b[0] <= a[0] for a, b in zip(amostras, amostras[1:])):
            raise ValueError("Os tempos das amostras devem ser crescentes.")
        if registro["unidade"] not in ("N", "kgf"):
            raise ValueError("Unidade de empuxo desconhecida.")
        return cls(
            registro["id"], registro["nome"], date.fromisoformat(registro["data"]),
            registro["observacoes"], registro["unidade"], amostras,
        )


class MemoriaDemonstracao:
    """A exclusão modifica apenas esta sessão; o arquivo de exemplos é preservado."""

    def __init__(self, testes):
        self._testes = {teste.identificador: teste for teste in testes}
        if len(self._testes) != len(testes):
            raise ValueError("Os identificadores dos testes precisam ser únicos.")

    @classmethod
    def carregar_exemplos(cls):
        caminho = Path(__file__).with_name("dados_demonstracao.json")
        registros = json.loads(caminho.read_text(encoding="utf-8"))
        return cls([TesteSalvo.de_dict(registro) for registro in registros])

    def listar(self):
        return sorted(self._testes.values(), key=lambda teste: (teste.data, teste.identificador), reverse=True)

    def obter(self, identificador):
        return self._testes[identificador]

    def excluir(self, identificadores):
        alvos = set(identificadores)
        if not alvos.issubset(self._testes):
            raise ValueError("Um dos registros selecionados não está mais na memória.")
        for identificador in alvos:
            del self._testes[identificador]
        return len(alvos)


def numero(valor, casas=1):
    return f"{valor:.{casas}f}".replace(".", ",")
