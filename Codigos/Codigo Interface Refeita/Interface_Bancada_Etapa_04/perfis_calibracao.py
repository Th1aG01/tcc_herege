"""Persistência local dos perfis. Não calcula calibração nem acessa hardware."""

from dataclasses import asdict, dataclass
from datetime import datetime
import json
import math
import os
from pathlib import Path
import sys
import tempfile
from uuid import uuid4


class ErroPerfis(Exception):
    pass


def caminho_padrao():
    if sys.platform == "win32":
        base = Path(os.environ.get("LOCALAPPDATA") or Path.home() / "AppData" / "Local")
    elif sys.platform == "darwin":
        base = Path.home() / "Library" / "Application Support"
    else:
        base = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local" / "share")
    return base / "BRAVO" / "BancadaEmpuxo" / "perfis_calibracao.json"


def valor_numerico(valor, rotulo, opcional=False):
    if valor is None or str(valor).strip() == "":
        if opcional:
            return None
        raise ValueError(f"Preencha {rotulo}.")
    if isinstance(valor, bool):
        raise ValueError(f"Informe um número válido em {rotulo}.")
    texto = str(valor).strip()
    if "," in texto and "." in texto:
        raise ValueError(f"Use vírgula ou ponto decimal, sem separador de milhar, em {rotulo}.")
    try:
        numero = float(texto.replace(",", "."))
    except ValueError:
        raise ValueError(f"Informe um número válido em {rotulo}.") from None
    if not math.isfinite(numero):
        raise ValueError(f"Informe um número finito em {rotulo}.")
    return numero


@dataclass(frozen=True)
class PerfilCalibracao:
    identificador: str
    nome: str
    celula: str
    capacidade_kgf: float | None
    coeficiente: float
    offset: int | None
    unidade: str
    observacoes: str
    salvo_em: str

    @classmethod
    def dos_campos(cls, campos, identificador=None, salvo_em=None):
        nome = str(campos.get("nome", "")).strip()
        if not nome:
            raise ValueError("Preencha o nome do perfil.")
        if len(nome) > 80:
            raise ValueError("Use até 80 caracteres no nome do perfil.")
        coeficiente = valor_numerico(campos.get("coeficiente"), "coeficiente de calibração")
        if coeficiente == 0:
            raise ValueError("O coeficiente de calibração não pode ser zero.")
        capacidade = valor_numerico(campos.get("capacidade_kgf"), "capacidade", opcional=True)
        if capacidade is not None and capacidade <= 0:
            raise ValueError("A capacidade deve ser maior que zero.")
        offset = valor_numerico(campos.get("offset"), "offset", opcional=True)
        if offset is not None and not offset.is_integer():
            raise ValueError("O offset deve ser uma contagem inteira ou ficar em branco.")
        unidade = campos.get("unidade", "")
        if unidade not in ("N", "kgf"):
            raise ValueError("Escolha a unidade N ou kgf.")
        return cls(
            identificador or uuid4().hex, nome, str(campos.get("celula", "")).strip(),
            capacidade, coeficiente, None if offset is None else int(offset), unidade,
            str(campos.get("observacoes", "")).strip(),
            salvo_em or datetime.now().astimezone().isoformat(timespec="seconds"),
        )


class BancoPerfis:
    def __init__(self, caminho=None):
        self.caminho = Path(caminho) if caminho is not None else caminho_padrao()
        self._perfis = {}
        self._conteudo_lido = None
        self.erro_leitura = None
        try:
            self._ler()
        except (OSError, ValueError, TypeError, KeyError) as erro:
            self.erro_leitura = f"Não foi possível ler os perfis salvos. O arquivo foi preservado. Detalhes: {erro}"

    def _ler(self):
        try:
            conteudo = self.caminho.read_bytes()
        except FileNotFoundError:
            return
        documento = json.loads(conteudo)
        if not isinstance(documento, dict) or documento.get("versao") != 1 or not isinstance(documento.get("perfis"), list):
            raise ValueError("Formato de arquivo não reconhecido.")
        perfis = {}
        nomes = set()
        for item in documento["perfis"]:
            ident = item["identificador"]
            if not isinstance(ident, str) or not ident or ident in perfis:
                raise ValueError("Identificador inválido ou repetido.")
            datetime.fromisoformat(item["salvo_em"])
            perfil = PerfilCalibracao.dos_campos(item, ident, item["salvo_em"])
            if perfil.nome.casefold() in nomes:
                raise ValueError("Há perfis com o mesmo nome.")
            nomes.add(perfil.nome.casefold())
            perfis[ident] = perfil
        self._perfis = perfis
        self._conteudo_lido = conteudo

    def listar(self):
        return sorted(self._perfis.values(), key=lambda perfil: perfil.nome.casefold())

    def obter(self, identificador):
        return self._perfis[identificador]

    def salvar(self, campos, identificador=None):
        if self.erro_leitura:
            raise ErroPerfis(self.erro_leitura)
        if identificador is not None and identificador not in self._perfis:
            raise ValueError("O perfil editado não foi encontrado.")
        perfil = PerfilCalibracao.dos_campos(campos, identificador)
        if any(p.nome.casefold() == perfil.nome.casefold() and p.identificador != perfil.identificador
               for p in self._perfis.values()):
            raise ValueError("Já existe um perfil com esse nome. Escolha outro nome ou edite o perfil existente.")
        proposta = dict(self._perfis)
        proposta[perfil.identificador] = perfil
        self._gravar(proposta)
        self._perfis = proposta
        return perfil

    def _gravar(self, perfis):
        temporario = None
        try:
            try:
                atual = self.caminho.read_bytes()
            except FileNotFoundError:
                atual = None
            if atual != self._conteudo_lido:
                raise ErroPerfis("O arquivo de perfis foi alterado fora desta janela. Reabra a interface antes de salvar.")
            documento = {"versao": 1, "perfis": [asdict(p) for p in perfis.values()]}
            conteudo = (json.dumps(documento, ensure_ascii=False, indent=2, allow_nan=False) + "\n").encode("utf-8")
            self.caminho.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile(mode="wb", dir=self.caminho.parent, prefix="perfis_", suffix=".tmp", delete=False) as arquivo:
                temporario = Path(arquivo.name)
                arquivo.write(conteudo)
                arquivo.flush()
                os.fsync(arquivo.fileno())
            os.replace(temporario, self.caminho)
            self._conteudo_lido = conteudo
        except OSError as erro:
            raise ErroPerfis(f"Não foi possível salvar os perfis: {erro}") from erro
        finally:
            if temporario is not None:
                try:
                    temporario.unlink(missing_ok=True)
                except OSError:
                    pass
