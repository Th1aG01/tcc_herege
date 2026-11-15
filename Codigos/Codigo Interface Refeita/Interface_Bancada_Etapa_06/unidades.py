"""Unidade fixa da interface e compatibilidade com registros anteriores."""

UNIDADE_EMPUXO = "kgf"
NEWTONS_POR_KGF = 9.80665


def empuxo_em_kgf(valor, unidade):
    if unidade == "kgf":
        return valor
    if unidade == "N":
        return valor / NEWTONS_POR_KGF
    raise ValueError("Unidade de empuxo desconhecida no arquivo.")


def coeficiente_em_kgf(valor, unidade):
    """Compatibilidade: força = leitura corrigida / coeficiente."""
    if unidade == "kgf":
        return valor
    if unidade == "N":
        return valor * NEWTONS_POR_KGF
    raise ValueError("Unidade de calibração desconhecida no arquivo.")
