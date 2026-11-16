/*
 * TCC BRAVO - Diagnostico de leitura SPI com o modulo LoRa SX127x
 * Isolado do firmware V0.18. NENHUM comando de escrita/apagamento da Flash.
 * NENHUMA transmissao de radio. Apenas leitura do registrador RegVersion 0x42.
 *
 * Antes de ligar: desconectar COMPLETAMENTE o modulo Flash;
 * conectar somente o LoRa com: CS=GPIO21, SCK=18, MISO=19, MOSI=23,
 * RST=14, VCC=3V3 e GND=GND. DIO0 nao e necessario para este teste.
 * Manter a ignicao fisicamente desconectada.
 */
#include <Arduino.h>
#include <SPI.h>

static constexpr int LORA_CS  = 21;
static constexpr int LORA_RST = 14;
static constexpr int PIN_SCK  = 18;
static constexpr int PIN_MISO = 19;
static constexpr int PIN_MOSI = 23;

static uint8_t lerVersao(uint32_t frequenciaHz) {
  SPI.beginTransaction(SPISettings(frequenciaHz, MSBFIRST, SPI_MODE0));
  digitalWrite(LORA_CS, LOW);
  SPI.transfer(0x42);                // Bit 7 em zero = LEITURA do RegVersion
  const uint8_t resposta = SPI.transfer(0x00);
  digitalWrite(LORA_CS, HIGH);
  SPI.endTransaction();
  return resposta;
}

void setup() {
  Serial.begin(115200);
  pinMode(LORA_CS, OUTPUT);
  digitalWrite(LORA_CS, HIGH);
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, HIGH);
  delay(100);

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, LORA_CS);

  // Reinicializacao fisica do SX127x (sem transmitir ou escrever registradores).
  digitalWrite(LORA_RST, LOW);
  delay(10);
  digitalWrite(LORA_RST, HIGH);
  delay(20);

  Serial.println("=== TCC BRAVO - DIAGNOSTICO SPI LORA SOMENTE LEITURA ===");
  Serial.println("Flash desconectada. LoRa no SPI. Sem transmissao de radio.");
  const uint32_t frequencias[] = {100000UL, 1000000UL};
  for (uint32_t hz : frequencias) {
    for (int i = 1; i <= 5; ++i) {
      const uint8_t versao = lerVersao(hz);
      Serial.printf("SPI %lu Hz - leitura %d: RegVersion=0x%02X %s\n",
                    static_cast<unsigned long>(hz), i, versao,
                    versao == 0x12 ? "(SX1276/77/78/79: esperado)" : "(verificar)");
      delay(150);
    }
  }
  Serial.println("Fim. Nenhuma escrita ou apagamento de Flash; nenhuma transmissao LoRa.");
}

void loop() {}
