#pragma once
#ifdef BANCADA_TESTE_ARQUIVOS
#include <backend_mock.h>
#else
#include "FlashNor.h"
#include "src/littlefs/lfs.h"
#include <memory>
#include <string>
#include <errno.h>
#include <new>

constexpr int FLASH_LEITURA = 0, FLASH_NOVO = 1;
class BackendExterno;
struct EstadoArquivoFlash {
  BackendExterno *dono = nullptr;
  lfs_file_t arquivo = {};
  lfs_dir_t diretorio = {};
  lfs_file_config config = {};
  uint8_t cache[256] = {};
  bool aberto = false, dir = false, escrita = false, falhou = false;
  std::string caminho;
  bool fechar();
  ~EstadoArquivoFlash() { fechar(); }
};
class ArquivoFlash {
  friend class BackendExterno;
  std::shared_ptr<EstadoArquivoFlash> h;
 public:
  explicit operator bool() const { return h && h->aberto; }
  bool isDirectory() const { return *this && h->dir; }
  size_t size() const;
  bool seek(size_t pos);
  size_t read(uint8_t *p, size_t n);
  size_t write(const uint8_t *p, size_t n);
  bool flush();
  bool close() { bool ok = !h || h->fechar(); h.reset(); return ok; }
  std::string getNextFileName(bool *dir);
};

class BackendExterno {
 public:
  FlashNor nor;
  lfs_t fs = {};
  lfs_config cfg = {};
  bool montado = false;
  int ultimoErro = 0;
  uint8_t leitura[256] = {}, programa[256] = {}, lookahead[64] = {};
  size_t espacoCache = 0, pendenteEspaco = 0;
  uint32_t ultimoEspaco = 0;
  bool espacoValido = false;
  uint32_t escritores = 0;

  BackendExterno() {
    cfg.context = this;
    cfg.read = ler; cfg.prog = gravar; cfg.erase = apagar; cfg.sync = sincronizar;
    cfg.read_size = 16; cfg.prog_size = 256; cfg.block_size = 4096;
    cfg.block_count = FLASH_CAPACIDADE / 4096; cfg.block_cycles = 100;
    cfg.cache_size = 256; cfg.lookahead_size = 64;
    cfg.read_buffer = leitura; cfg.prog_buffer = programa; cfg.lookahead_buffer = lookahead;
    cfg.name_max = 63; cfg.file_max = FLASH_CAPACIDADE; cfg.attr_max = 0;
  }
  BackendExterno(const BackendExterno &) = delete;
  BackendExterno &operator=(const BackendExterno &) = delete;
  bool escritaHabilitada() const { return nor.escrita && !nor.falha && montado; }
  const char *diagnostico() const {
    if (nor.falha || !nor.identificada) return nor.erro;
    if (!montado) return "FS_NAO_MONTADO";
    if (ultimoErro < 0) return "FS_ERRO";
    return nor.escrita ? "OK" : "SOMENTE_LEITURA";
  }
  bool resultado(int r) { if (r < 0) { ultimoErro = r; return false; } return true; }
  static BackendExterno &self(const lfs_config *c) { return *static_cast<BackendExterno *>(c->context); }
  static bool valido(const lfs_config *c, lfs_block_t b, lfs_off_t o, lfs_size_t n) {
    return b < c->block_count && o <= c->block_size && n <= c->block_size - o;
  }
  static int ler(const lfs_config *c, lfs_block_t b, lfs_off_t o, void *p, lfs_size_t n) {
    return valido(c,b,o,n) && self(c).nor.read(b*4096+o,p,n) ? 0 : LFS_ERR_IO;
  }
  static int gravar(const lfs_config *c, lfs_block_t b, lfs_off_t o, const void *p, lfs_size_t n) {
    return valido(c,b,o,n) && self(c).nor.program(b*4096+o,p,n) ? 0 : LFS_ERR_IO;
  }
  static int apagar(const lfs_config *c, lfs_block_t b) {
    return valido(c,b,0,4096) && self(c).nor.erase(b*4096) ? 0 : LFS_ERR_IO;
  }
  static int sincronizar(const lfs_config *c) { return self(c).nor.sync() ? 0 : LFS_ERR_IO; }
  bool begin(bool formatar = false) {
    // Nem o parametro true autoriza formatacao. API compatibilidade da base.
    if (formatar || montado) return montado && !formatar;
    ultimoErro = 0;
    if (!nor.begin()) return false;
    montado = resultado(lfs_mount(&fs, &cfg));
    espacoValido = false; return montado;
  }
  bool end() {
    if (!montado) return true;
    bool ok = resultado(lfs_unmount(&fs)); montado = false; return ok;
  }
  bool exists(const char *p) {
    lfs_info info = {};
    if (!montado) return false;
    int r = lfs_stat(&fs,p,&info);
    if (r != LFS_ERR_NOENT) resultado(r);
    return r == 0;
  }
  bool mkdir(const char *p) {
    if (!escritaHabilitada()) return false;
    espacoValido = false; return resultado(lfs_mkdir(&fs,p));
  }
  bool remove(const char *p) {
    if (!escritaHabilitada()) return false;
    espacoValido = false; return resultado(lfs_remove(&fs,p));
  }
  size_t totalBytes() const { return FLASH_CAPACIDADE; }
  size_t usedBytes() {
    if (!montado || nor.falha) return FLASH_CAPACIDADE + 1;
    if (!espacoValido || (!escritores && (uint32_t)((uint32_t)millis()-ultimoEspaco) >= 1000)) {
      lfs_ssize_t blocos = lfs_fs_size(&fs);
      if (!resultado(blocos)) return FLASH_CAPACIDADE + 1;
      espacoCache = (size_t)blocos * 4096; pendenteEspaco = 0;
      ultimoEspaco = millis(); espacoValido = true;
    }
    size_t estimativa = espacoCache + pendenteEspaco;
    return estimativa < FLASH_CAPACIDADE ? estimativa : FLASH_CAPACIDADE;
  }
  ArquivoFlash open(const char *p, int modo = FLASH_LEITURA) {
    ArquivoFlash f;
    if (!montado || (modo == FLASH_NOVO && !escritaHabilitada())) return f;
    // Fazer a varredura antes de iniciar o escritor; evitar catalogacao por amostra.
    if (modo == FLASH_NOVO && usedBytes() > FLASH_CAPACIDADE) return f;
    auto *estado = new(std::nothrow) EstadoArquivoFlash;
    if (!estado) { ultimoErro = LFS_ERR_NOMEM; return f; }
    f.h.reset(estado); estado->dono = this; estado->caminho = p;
    lfs_info info = {};
    int r = lfs_stat(&fs,p,&info);
    if (r < 0 && r != LFS_ERR_NOENT) { resultado(r); f.h.reset(); return f; }
    estado->dir = r == 0 && info.type == LFS_TYPE_DIR;
    if (estado->dir && modo == FLASH_LEITURA) r = lfs_dir_open(&fs,&estado->diretorio,p);
    else if (estado->dir) r = LFS_ERR_ISDIR;
    else {
      estado->config.buffer = estado->cache;
      // EXCL, nunca TRUNC: protege arquivos existentes inclusive em falha de exists().
      int flags = modo == FLASH_NOVO ? LFS_O_WRONLY|LFS_O_CREAT|LFS_O_EXCL : LFS_O_RDONLY;
      r = lfs_file_opencfg(&fs,&estado->arquivo,p,flags,&estado->config);
    }
    if (!resultado(r)) { f.h.reset(); return f; }
    estado->aberto = true; estado->escrita = modo == FLASH_NOVO;
    if (estado->escrita) { ++escritores; pendenteEspaco += 4096; }
    return f;
  }
#ifdef BANCADA_PROVISIONAMENTO
  // Somente no utilitario separado. Nunca chamado pelo firmware principal.
  bool prepararVazia(bool autorizado) {
    if (!autorizado || montado || !nor.escrita || !nor.begin()) return false;
    uint8_t pagina[256];
    for (uint32_t a = 0; a < FLASH_CAPACIDADE; a += sizeof(pagina)) {
      if (!nor.read(a,pagina,sizeof(pagina))) return false;
      for (uint8_t b : pagina) if (b != 0xff) { ultimoErro = LFS_ERR_EXIST; return false; }
    }
    if (!resultado(lfs_format(&fs,&cfg)) || !begin(false)) return false;
    return mkdir("/ensaios");
  }
#endif
};

inline bool EstadoArquivoFlash::fechar() {
  if (!aberto) return !falhou;
  int r = dir ? lfs_dir_close(&dono->fs,&diretorio) : lfs_file_close(&dono->fs,&arquivo);
  aberto = false;
  if (escrita) { --dono->escritores; dono->espacoValido = false; }
  if (!dono->resultado(r)) falhou = true;
  return !falhou;
}
inline size_t ArquivoFlash::size() const {
  if (!*this || h->dir) return 0;
  lfs_soff_t n = lfs_file_size(&h->dono->fs,&h->arquivo);
  return h->dono->resultado(n) ? (size_t)n : 0;
}
inline bool ArquivoFlash::seek(size_t pos) {
  if (!*this || h->dir || pos > size()) return false;
  return h->dono->resultado(lfs_file_seek(&h->dono->fs,&h->arquivo,pos,LFS_SEEK_SET));
}
inline size_t ArquivoFlash::read(uint8_t *p, size_t n) {
  if (!*this || h->dir) return 0;
  lfs_ssize_t r = lfs_file_read(&h->dono->fs,&h->arquivo,p,n);
  return h->dono->resultado(r) ? (size_t)r : 0;
}
inline size_t ArquivoFlash::write(const uint8_t *p, size_t n) {
  if (!*this || h->dir || h->falhou || !h->dono->escritaHabilitada()) return 0;
  lfs_ssize_t r = lfs_file_write(&h->dono->fs,&h->arquivo,p,n);
  if (!h->dono->resultado(r) || (size_t)r != n) { h->falhou = true; return 0; }
  h->dono->pendenteEspaco += 2*n; // margem conservadora entre varreduras de espaco.
  return r;
}
inline bool ArquivoFlash::flush() {
  if (!*this || h->dir || h->falhou) return false;
  bool ok = h->dono->resultado(lfs_file_sync(&h->dono->fs,&h->arquivo));
  if (!ok) h->falhou = true;
  return ok;
}
inline std::string ArquivoFlash::getNextFileName(bool *dir) {
  if (!*this || !h->dir) { errno = EBADF; return {}; }
  lfs_info info = {};
  int r;
  do { r = lfs_dir_read(&h->dono->fs,&h->diretorio,&info); }
  while (r > 0 && (!strcmp(info.name,".") || !strcmp(info.name,"..")));
  if (r < 0) { errno = EIO; h->dono->resultado(r); return {}; }
  if (!r) return {};
  *dir = info.type == LFS_TYPE_DIR;
  return h->caminho + "/" + info.name;
}
static BackendExterno FlashEnsaios;
#endif
