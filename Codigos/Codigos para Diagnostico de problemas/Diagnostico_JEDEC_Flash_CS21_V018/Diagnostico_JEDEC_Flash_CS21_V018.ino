/*
 * TCC BRAVO - V0.18 - Diagnostico cruzado SPI (Flash no CS do LoRa)
 * Versao do diagnostico: 1.0 (2026-11-16)
 * Objetivo: testar a W25Q16 na mesma selecao GPIO21 usada no teste LoRa 10/10.
 * APENAS LEITURA JEDEC 0x9F - nao escreve, apaga, formata ou altera o firmware V0.18.
 * Flash = unica periferica SPI externa conectada durante o teste.
 * CS=21, SCK=18, MISO=19, MOSI=23, WP=3V3, HOLD=3V3, VCC=3V3, GND=GND.
 * Ignicao FISICAMENTE desconectada.
 */
#include <Arduino.h>
#include <SPI.h>

static constexpr uint8_t FLASH_CS = 21;
static constexpr uint8_t PIN_SCK = 18;
static constexpr uint8_t PIN_MISO = 19;
static constexpr uint8_t PIN_MOSI = 23;

static uint32_t lerJedec(uint32_t frequenciaHz) {
  SPI.beginTransaction(SPISettings(frequenciaHz, MSBFIRST, SPI_MODE0));
  digitalWrite(FLASH_CS, LOW);
  SPI.transfer(0x9F); // Read JEDEC ID - nao grava na memoria.
  const uint8_t fabricante = SPI.transfer(0x00);
  const uint8_t tipo = SPI.transfer(0x00);
  const uint8_t capacidade = SPI.transfer(0x00);
  digitalWrite(FLASH_CS, HIGH);
  SPI.endTransaction();
  return (uint32_t(fabricante) << 16) | (uint32_t(tipo) << 8) | capacidade;
}

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_CS, OUTPUT);
  digitalWrite(FLASH_CS, HIGH);
  delay(300);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, FLASH_CS);
  Serial.println("=== TCC BRAVO - FLASH JEDEC CS GPIO21 - SOMENTE LEITURA ===");
  Serial.println("LoRa deve estar desconectado. Flash no caminho antes testado com LoRa.");
  Serial.println("Esperado para o dispositivo anteriormente identificado: EF 40 15.");
  const uint32_t frequencias[] = {100000UL, 1000000UL};
  for (const uint32_t hz : frequencias) {
    for (int i = 1; i <= 5; ++i) {
      const uint32_t jedec = lerJedec(hz);
      Serial.printf("SPI %lu Hz - leitura %d: JEDEC=%02X %02X %02X%s\n",
                    (unsigned long)hz, i,
                    (unsigned)((jedec >> 16) & 0xFF),
                    (unsigned)((jedec >> 8) & 0xFF),
                    (unsigned)(jedec & 0xFF),
                    jedec == 0xEF4015UL ? " (esperado)" : " (invalido/inesperado)");
      delay(150);
    }
  }
  Serial.println("Fim. Nenhuma operacao de escrita, apagamento ou formatacao.");
}

void loop() {}
