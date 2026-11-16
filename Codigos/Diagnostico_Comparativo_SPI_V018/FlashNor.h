#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>
#include <string.h>
#include "ConfiguracaoFlash.h"

// Unico dono: loop/setup. Nao chamar de ISR ou de outra task.
class FlashNor {
 public:
  uint32_t jedec = 0;
  const char *erro = "NAO_INICIADA";
  bool identificada = false;
  bool escrita = FLASH_PERMITIR_ESCRITA != 0;
  bool falha = false;
  uint32_t programas = 0, apagamentos = 0, esperaMaxMs = 0;

  void selecionar() {
    SPI.beginTransaction(SPISettings(FLASH_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(FLASH_CS, LOW);
  }
  void liberar() { digitalWrite(FLASH_CS, HIGH); SPI.endTransaction(); }
  uint8_t status(uint8_t comando = 0x05) {
    selecionar(); SPI.transfer(comando); uint8_t s = SPI.transfer(0xff); liberar(); return s;
  }
  bool rejeitar(const char *motivo) { erro = motivo; falha = true; return false; }
  bool aguardar(uint32_t limite = 600) {
    uint32_t inicio = millis();
    do {
      uint8_t s = status();
      if (s == 0xff) return rejeitar("FLASH_SEM_RESPOSTA");
      if (!(s & 1)) {
        uint32_t t = (uint32_t)millis() - inicio;
        if (t > esperaMaxMs) esperaMaxMs = t;
        return true;
      }
      delay(1); // CS alto, barramento livre; nao desabilitar interrupcoes.
    } while ((uint32_t)((uint32_t)millis() - inicio) < limite);
    return rejeitar("FLASH_BUSY_TIMEOUT");
  }
  bool begin() {
    falha = false; identificada = false;
    selecionar(); SPI.transfer(0x9f);
    jedec = (uint32_t)SPI.transfer(0xff) << 16;
    jedec |= (uint32_t)SPI.transfer(0xff) << 8;
    jedec |= SPI.transfer(0xff); liberar();
    if (jedec != FLASH_JEDEC_ESPERADO) return rejeitar("JEDEC_NAO_SUPORTADO");
    if (!aguardar()) return false;
    identificada = true; erro = "OK"; return true;
  }
  bool limites(uint32_t endereco, size_t n) const {
    return endereco <= FLASH_CAPACIDADE && n <= FLASH_CAPACIDADE - endereco;
  }
  void endereco24(uint32_t a) {
    SPI.transfer((uint8_t)(a >> 16)); SPI.transfer((uint8_t)(a >> 8)); SPI.transfer((uint8_t)a);
  }
  bool read(uint32_t a, void *destino, size_t n) {
    if (!identificada || !limites(a, n)) return rejeitar("FLASH_LEITURA_LIMITE");
    if (!aguardar()) return false;
    auto *p = static_cast<uint8_t *>(destino);
    // Transacoes curtas mesmo para varredura de setores.
    while (n) {
      size_t parte = n > 256 ? 256 : n;
      selecionar(); SPI.transfer(0x03); endereco24(a);
      for (size_t i = 0; i < parte; ++i) p[i] = SPI.transfer(0xff);
      liberar(); a += parte; p += parte; n -= parte;
    }
    return true;
  }
  bool habilitarGravacao() {
    if (!identificada || !escrita || falha) { erro = "FLASH_ESCRITA_BLOQUEADA"; return false; }
    if (!aguardar()) return false;
    // Nao remover protecoes do chip nem alterar registradores de status.
    if ((status() & 0x1c) || (status(0x35) & 0x40)) return rejeitar("FLASH_PROTEGIDA");
    selecionar(); SPI.transfer(0x06); liberar();
    if (!(status() & 2)) return rejeitar("FLASH_WEL_FALHOU");
    return true;
  }
  bool program(uint32_t a, const void *dados, size_t n) {
    if (!limites(a, n)) return rejeitar("FLASH_PROGRAMA_LIMITE");
    const auto *p = static_cast<const uint8_t *>(dados);
    uint8_t comparar[256];
    while (n) {
      size_t parte = 256 - (a & 255); if (parte > n) parte = n;
      if (!read(a, comparar, parte)) return false;
      for (size_t i = 0; i < parte; ++i)
        if ((comparar[i] & p[i]) != p[i]) return rejeitar("FLASH_REQUER_ERASE");
      if (!habilitarGravacao()) return false;
      selecionar(); SPI.transfer(0x02); endereco24(a);
      for (size_t i = 0; i < parte; ++i) SPI.transfer(p[i]);
      liberar(); ++programas;
      if (!aguardar(20) || !read(a, comparar, parte)) return false;
      if (memcmp(comparar, p, parte)) return rejeitar("FLASH_PROGRAMA_VERIFICACAO");
      a += parte; p += parte; n -= parte;
    }
    return true;
  }
  bool erase(uint32_t a) {
    if ((a & 4095) || !limites(a, 4096)) return rejeitar("FLASH_ERASE_LIMITE");
    if (!habilitarGravacao()) return false;
    selecionar(); SPI.transfer(0x20); endereco24(a); liberar(); ++apagamentos;
    if (!aguardar()) return false;
    uint8_t bloco[256];
    for (uint32_t off = 0; off < 4096; off += sizeof(bloco)) {
      if (!read(a + off, bloco, sizeof(bloco))) return false;
      for (uint8_t b : bloco) if (b != 0xff) return rejeitar("FLASH_ERASE_VERIFICACAO");
    }
    return true;
  }
  bool sync() { return identificada && !falha && aguardar(); }
};
