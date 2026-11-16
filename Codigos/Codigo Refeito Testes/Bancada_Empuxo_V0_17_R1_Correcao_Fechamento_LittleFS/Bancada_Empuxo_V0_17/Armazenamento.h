#pragma once
// LittleFS interno. Nenhuma rotina deste arquivo formata a particao.
#include <stddef.h>
#include <errno.h>

const char *DIRETORIO_ENSAIOS = "/ensaios";
const char *ID_LEGADO = "LEGADO_ENSAIO_BIN";
const uint32_t MAGIC_ENSAIO = 0x42454E33, MAGIC_FIM = 0x46494D33;
const uint32_t FORMATO_GRAVACAO = 3;
const size_t RESERVA_FLASH = 8192; // margem para metadados/blocos do LittleFS
const uint8_t LIMITE_BLOCO = 4;

struct Payload {
  uint32_t tempo_ms;
  uint32_t indice;
  double empuxo;
  double fatorCalibracao;
  char ensaioId[32];
};
struct CabecalhoEnsaio {
  uint32_t magic, formato, cabecalhoBytes, amostraBytes;
  char ensaioId[32];
  double coeficiente;
  int32_t taraInterna;
  char unidade[4];
  char nome[64], data[16], observacoes[160], celulaId[40], perfilId[40];
  uint32_t reservado, crc;
};
struct RegistroAmostra {
  Payload dados;
  uint32_t reservado, crc;
};
struct RodapeEnsaio {
  uint32_t magic, resultado, amostras, duracao_ms; // resultado 1 normal, 2 interrompido
  char motivo[32];
  uint32_t amostraBytes, crc;
};
static_assert(sizeof(Payload) == 56, "Layout Payload");
static_assert(sizeof(CabecalhoEnsaio) == 392 && offsetof(CabecalhoEnsaio, crc) == 388, "Layout cabecalho");
static_assert(sizeof(RegistroAmostra) == 64 && offsetof(RegistroAmostra, crc) == 60, "Layout amostra");
static_assert(sizeof(RodapeEnsaio) == 56 && offsetof(RodapeEnsaio, crc) == 52, "Layout rodape");

uint32_t crc32(const void *dados, size_t n) {
  const uint8_t *p = (const uint8_t *)dados;
  uint32_t crc = 0xffffffffU;
  while (n--) {
    crc ^= *p++;
    for (uint8_t bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}

bool idEnsaioValido(const char *id) {
  size_t n = strlen(id);
  if (!n || n > 31) return false;
  for (size_t i = 0; i < n; i++) {
    char c = id[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
  }
  return true;
}

void caminhoEnsaio(const char *id, char *caminho, size_t n) {
  if (!strcmp(id, ID_LEGADO)) snprintf(caminho, n, "/ensaio.bin");
  else snprintf(caminho, n, "%s/%s.ben", DIRETORIO_ENSAIOS, id);
}

bool idDoArquivo(const char *nome, char *id) {
  const char *base = strrchr(nome, '/');
  base = base ? base + 1 : nome;
  size_t n = strlen(base);
  if (n < 5 || n > 35 || strcmp(base + n - 4, ".ben")) return false;
  memcpy(id, base, n - 4); id[n - 4] = 0;
  return idEnsaioValido(id) && strcmp(id, ID_LEGADO);
}

bool cabecalhoValido(const CabecalhoEnsaio &cab) {
  return cab.magic == MAGIC_ENSAIO && cab.formato == FORMATO_GRAVACAO &&
    cab.cabecalhoBytes == sizeof(cab) && cab.amostraBytes == sizeof(RegistroAmostra) &&
    cab.crc == crc32(&cab, offsetof(CabecalhoEnsaio, crc)) &&
    memchr(cab.ensaioId, 0, sizeof(cab.ensaioId)) && idEnsaioValido(cab.ensaioId) &&
    isfinite(cab.coeficiente) && cab.coeficiente != 0 && !memcmp(cab.unidade, "kgf", 4) &&
    memchr(cab.nome, 0, sizeof(cab.nome)) && memchr(cab.data, 0, sizeof(cab.data)) &&
    memchr(cab.observacoes, 0, sizeof(cab.observacoes)) &&
    memchr(cab.celulaId, 0, sizeof(cab.celulaId)) && memchr(cab.perfilId, 0, sizeof(cab.perfilId));
}

bool lerFechamento(File &arquivo, CabecalhoEnsaio &cab, RodapeEnsaio &fim) {
  if (!arquivo || arquivo.size() < sizeof(cab) || !arquivo.seek(0) ||
      arquivo.read((uint8_t *)&cab, sizeof(cab)) != sizeof(cab) || !cabecalhoValido(cab) ||
      arquivo.size() < sizeof(cab) + sizeof(fim) || !arquivo.seek(arquivo.size() - sizeof(fim)) ||
      arquivo.read((uint8_t *)&fim, sizeof(fim)) != sizeof(fim)) return false;
  return fim.magic == MAGIC_FIM && (fim.resultado == 1 || fim.resultado == 2) &&
    fim.crc == crc32(&fim, offsetof(RodapeEnsaio, crc)) && fim.amostraBytes == sizeof(RegistroAmostra) &&
    memchr(fim.motivo, 0, sizeof(fim.motivo)) &&
    (uint64_t)arquivo.size() == sizeof(cab) + (uint64_t)fim.amostras * sizeof(RegistroAmostra) + sizeof(fim) &&
    (fim.resultado != 1 || fim.amostras > 0);
}

bool lerAmostra(File &arquivo, const CabecalhoEnsaio &cab, uint32_t indice, Payload &p) {
  if (!indice) return false;
  uint64_t pos = sizeof(cab) + (uint64_t)(indice - 1) * sizeof(RegistroAmostra);
  RegistroAmostra registro = {};
  if (pos > UINT32_MAX || !arquivo.seek((uint32_t)pos) ||
      arquivo.read((uint8_t *)&registro, sizeof(registro)) != sizeof(registro)) return false;
  p = registro.dados;
  return registro.crc == crc32(&registro, offsetof(RegistroAmostra, crc)) &&
    p.indice == indice && memchr(p.ensaioId, 0, sizeof(p.ensaioId)) &&
    !strcmp(p.ensaioId, cab.ensaioId) && p.fatorCalibracao == cab.coeficiente && isfinite(p.empuxo);
}

struct InformacaoArquivo {
  CabecalhoEnsaio cab = {};
  RodapeEnsaio fim = {};
  uint32_t formato = 0, slots = 0, ultimoTempo = 0;
  bool ultimoTempoValido = false;
  size_t bytes = 0;
  bool existe = false, cabecalhoOk = false, fechado = false;
  const char *condicao = "incompleto";
  const char *diagnostico = "CABECALHO_INVALIDO";
};

InformacaoArquivo inspecionarArquivo(const char *id) {
  InformacaoArquivo info;
  char caminho[64]; caminhoEnsaio(id, caminho, sizeof(caminho));
  File f = LittleFS.open(caminho, FILE_READ);
  if (!f || f.isDirectory()) { f.close(); return info; }
  info.existe = true; info.bytes = f.size();
  uint32_t prefixo[2] = {};
  if (f.read((uint8_t *)prefixo, sizeof(prefixo)) == sizeof(prefixo)) {
    if (prefixo[0] == MAGIC_ENSAIO || prefixo[0] == 0x42454E32) info.formato = prefixo[1];
  }
  if (!strcmp(id, ID_LEGADO)) {
    info.condicao = "legado";
    info.diagnostico = info.formato == 2 ? "LEGADO_FORMATO_2" : "LEGADO_SEM_VERSAO";
    f.close(); return info;
  }
  if (info.formato && info.formato != FORMATO_GRAVACAO) {
    info.condicao = "nao_suportado"; info.diagnostico = "FORMATO_NAO_SUPORTADO";
    f.close(); return info;
  }
  info.fechado = lerFechamento(f, info.cab, info.fim);
  info.cabecalhoOk = cabecalhoValido(info.cab) && !strcmp(info.cab.ensaioId, id);
  if (!info.cabecalhoOk) info.fechado = false;
  if (info.cabecalhoOk) {
    info.slots = (info.bytes - sizeof(info.cab)) / sizeof(RegistroAmostra);
    info.diagnostico = "SEM_FECHAMENTO_VALIDO";
    if (info.fechado) {
      info.slots = info.fim.amostras;
      info.condicao = info.fim.resultado == 1 ? "concluido" : "incompleto";
      info.diagnostico = info.fim.resultado == 1 ? "FECHAMENTO_NORMAL" : "ENCERRAMENTO_COM_FALHA";
    }
    Payload ultima = {};
    if (info.slots && lerAmostra(f, info.cab, info.slots, ultima)) {
      info.ultimoTempoValido = true; info.ultimoTempo = ultima.tempo_ms;
    }
  }
  f.close(); return info;
}

// Varredura cooperativa, independente da ordem interna do diretorio.
// Seleciona os menores IDs > cursor. Apenas uma entrada por chamada a passo().
struct PaginaCatalogo {
  File diretorio;
  char apos[32] = {};
  char ids[LIMITE_BLOCO][32] = {};
  uint8_t quantidade = 0, limite = LIMITE_BLOCO;
  bool mais = false, legadoPendente = true, concluida = false, falhou = false;

  bool iniciar(const char *cursor, uint8_t maximo) {
    diretorio.close();
    strcpy(apos, cursor); quantidade = 0; limite = maximo;
    mais = false; legadoPendente = true; concluida = false; falhou = false;
    diretorio = LittleFS.open(DIRETORIO_ENSAIOS, FILE_READ);
    if (!diretorio || !diretorio.isDirectory()) { diretorio.close(); return false; }
    return true;
  }
  void considerar(const char *id) {
    if (strcmp(id, apos) <= 0) return;
    uint8_t pos = 0;
    while (pos < quantidade && strcmp(ids[pos], id) < 0) pos++;
    if (pos < quantidade && !strcmp(ids[pos], id)) return;
    if (quantidade == limite) {
      mais = true;
      if (pos >= limite) return;
    } else quantidade++;
    for (uint8_t i = quantidade - 1; i > pos; i--) strcpy(ids[i], ids[i - 1]);
    strcpy(ids[pos], id);
  }
  void passo() {
    if (concluida) return;
    if (legadoPendente) {
      legadoPendente = false;
      if (LittleFS.exists("/ensaio.bin")) considerar(ID_LEGADO);
      return;
    }
    bool eDiretorio = false;
    errno = 0;
    auto nome = diretorio.getNextFileName(&eDiretorio);
    if (!nome.length()) {
      falhou = errno != 0; diretorio.close(); concluida = true; return;
    }
    char id[32];
    if (!eDiretorio && idDoArquivo(nome.c_str(), id)) considerar(id);
  }
};
