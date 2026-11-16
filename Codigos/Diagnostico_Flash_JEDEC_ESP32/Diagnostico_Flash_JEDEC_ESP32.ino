/*
  TCC BRAVO — Diagnostico JEDEC SOMENTE LEITURA
  Conexoes conforme esquemas recebidos em 15/11/2026:
  Flash CS=GPIO13, LoRa CS=GPIO21, SPI SCK=18, MISO=19, MOSI=23.
  Leitura via opcode 0x9F; NENHUM comando de escrita ou apagamento.
  Use apenas com motor/ignitor FISICAMENTE desconectado.
  Antes de gravar este sketch no ESP32, preserve os ensaios importantes
  armazenados na Flash INTERNA e mantenha o esquema de particoes atual.
*/
#include <Arduino.h>
#include <SPI.h>

static constexpr uint8_t FLASH_CS_PIN = 13;
static constexpr uint8_t LORA_CS_PIN = 21;
static constexpr uint8_t SPI_SCK_PIN = 18;
static constexpr uint8_t SPI_MISO_PIN = 19;
static constexpr uint8_t SPI_MOSI_PIN = 23;

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_CS_PIN, OUTPUT);
  pinMode(LORA_CS_PIN, OUTPUT);
  digitalWrite(FLASH_CS_PIN, HIGH); // Flash deselecionada
  digitalWrite(LORA_CS_PIN, HIGH);  // LoRa deselecionado
  delay(300);
  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, FLASH_CS_PIN);
  Serial.println("=== Diagnostico JEDEC da Flash externa (somente leitura) ===");
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(FLASH_CS_PIN, LOW);
  SPI.transfer(0x9F); // Read JEDEC ID, nao escreve no chip
  const uint8_t fabricante = SPI.transfer(0x00);
  const uint8_t tipo = SPI.transfer(0x00);
  const uint8_t capacidade = SPI.transfer(0x00);
  digitalWrite(FLASH_CS_PIN, HIGH);
  SPI.endTransaction();

  char resultado[80];
  snprintf(resultado, sizeof(resultado), "ID JEDEC: %02X %02X %02X", fabricante, tipo, capacidade);
  Serial.println(resultado);
  if ((fabricante == 0xFF && tipo == 0xFF && capacidade == 0xFF) ||
      (fabricante == 0x00 && tipo == 0x00 && capacidade == 0x00)) {
    Serial.println("ATENCAO: resposta invalida. Conferir alimentacao, CS, MISO, MOSI, SCK, WP e HOLD.");
  } else if (fabricante == 0xEF && tipo == 0x40 && capacidade == 0x15) {
    Serial.println("ID compativel com Winbond W25Q16 (16 Mbits). Conferir marcacao fisica.");
  } else {
    Serial.println("ID recebido. Identificar modelo/datasheet antes de usar comandos de gravacao.");
  }
  Serial.println("Nenhum dado foi escrito ou apagado na Flash externa.");
}

void loop() {}
