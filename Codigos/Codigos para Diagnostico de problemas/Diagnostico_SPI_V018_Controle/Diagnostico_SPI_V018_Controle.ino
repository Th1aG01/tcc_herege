/*
  TCC BRAVO - Diagnostico SPI comparativo controlado (v1)
  SOMENTE LEITURA JEDEC (0x9F). Sem gravacao, erase ou formatacao.
  Primeira transacao reproduz as operacoes do diagnostico JEDEC original.
  ESP32: Flash CS=13, LoRa CS=21, SCK=18, MISO=19, MOSI=23.
  Manter o circuito de ignicao FISICAMENTE desconectado.
*/
#include <Arduino.h>
#include <SPI.h>

static constexpr uint8_t FLASH_CS_PIN = 13;
static constexpr uint8_t LORA_CS_PIN = 21;
static constexpr uint8_t SPI_SCK_PIN = 18;
static constexpr uint8_t SPI_MISO_PIN = 19;
static constexpr uint8_t SPI_MOSI_PIN = 23;

static uint32_t leituraComparativa(uint8_t dummy, bool csRadioDentroTransacao) {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  if (csRadioDentroTransacao) digitalWrite(LORA_CS_PIN, HIGH);
  digitalWrite(FLASH_CS_PIN, LOW);
  SPI.transfer(0x9F);
  const uint8_t a = SPI.transfer(dummy);
  const uint8_t b = SPI.transfer(dummy);
  const uint8_t c = SPI.transfer(dummy);
  digitalWrite(FLASH_CS_PIN, HIGH);
  SPI.endTransaction();
  return (uint32_t(a) << 16) | (uint32_t(b) << 8) | c;
}

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_CS_PIN, OUTPUT);
  pinMode(LORA_CS_PIN, OUTPUT);
  digitalWrite(FLASH_CS_PIN, HIGH);
  digitalWrite(LORA_CS_PIN, HIGH);
  delay(300);
  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, FLASH_CS_PIN);
  Serial.println("=== Controle JEDEC: somente leitura ===");

  // A: mesmas operacoes SPI, mesma ordem e mesmos bytes do sketch que funcionou.
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(FLASH_CS_PIN, LOW);
  SPI.transfer(0x9F);
  const uint8_t fabricante = SPI.transfer(0x00);
  const uint8_t tipo = SPI.transfer(0x00);
  const uint8_t capacidade = SPI.transfer(0x00);
  digitalWrite(FLASH_CS_PIN, HIGH);
  SPI.endTransaction();
  Serial.printf("A original: %02X %02X %02X\n", fabricante, tipo, capacidade);

  delay(300);
  Serial.printf("B repeticao 00: %06lX\n", (unsigned long)leituraComparativa(0x00, false));
  delay(300);
  Serial.printf("C repeticao FF: %06lX\n", (unsigned long)leituraComparativa(0xFF, false));
  delay(300);
  Serial.printf("D CS radio dentro da transacao: %06lX\n",
                (unsigned long)leituraComparativa(0x00, true));

  for (int i = 1; i <= 5; ++i) {
    delay(300);
    Serial.printf("E repeticao %d: %06lX\n", i,
                  (unsigned long)leituraComparativa(0x00, false));
  }
  Serial.println("Fim. Sem comandos de escrita/apagamento da Flash externa.");
}
void loop() {}
