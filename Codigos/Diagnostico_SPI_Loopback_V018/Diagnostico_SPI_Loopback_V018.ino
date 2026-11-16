/*
 * TCC BRAVO - Diagnostico SPI Loopback V0.18 (isolado)
 * TESTE DO ESP32, NAO DA FLASH.
 * ANTES DE ENERGIZAR: desconectar TOTALMENTE Flash e LoRa.
 * Ligar SOMENTE GPIO23 (MOSI) diretamente ao GPIO19 (MISO).
 * Nao conectar a Flash durante o loopback (evita conflito no MISO).
 * Ignição fisicamente desconectada.
 * Nao utiliza comandos de memoria: nao escreve, apaga ou formata.
 */
#include <Arduino.h>
#include <SPI.h>

static constexpr int PIN_SCK = 18;
static constexpr int PIN_MISO = 19;
static constexpr int PIN_MOSI = 23;
static constexpr uint8_t PADROES[] = {0x00, 0xFF, 0x55, 0xAA, 0xA5, 0x5A, 0x01, 0x80, 0x3C, 0xC3};

static void testarLoopback(const char* rotulo, uint32_t hz) {
  unsigned long total = 0;
  unsigned long falhas = 0;
  SPI.beginTransaction(SPISettings(hz, MSBFIRST, SPI_MODE0));
  for (int repeticao = 0; repeticao < 8; ++repeticao) {
    for (size_t i = 0; i < sizeof(PADROES); ++i) {
      const uint8_t enviado = PADROES[i];
      const uint8_t recebido = SPI.transfer(enviado);
      ++total;
      if (recebido != enviado) {
        ++falhas;
        if (falhas <= 8) {
          Serial.printf("  ERRO: repeticao=%d TX=%02X RX=%02X\n", repeticao + 1, enviado, recebido);
        }
      }
    }
  }
  SPI.endTransaction();
  Serial.printf("%s (%lu Hz): %lu/%lu acertos - %s\n", rotulo,
                (unsigned long)hz, total - falhas, total,
                falhas == 0 ? "PASSOU" : "FALHOU");
}

void setup() {
  Serial.begin(115200);
  delay(700);
  Serial.println("=== TCC BRAVO - LOOPBACK SPI ESP32 V0.18 ===");
  Serial.println("Flash e LoRa devem estar fisicamente DESCONECTADOS.");
  Serial.println("Jumper unico de teste: GPIO23 (MOSI) -> GPIO19 (MISO).");
  // O CS nao e utilizado: a Flash deve estar fisicamente desconectada.
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, -1);
  testarLoopback("LOOPBACK 100 kHz", 100000);
  testarLoopback("LOOPBACK 1 MHz", 1000000);
  Serial.println("Fim. Nenhum comando enviado a memorias externas.");
  Serial.println("OBS: sucesso NAO verifica o pino SCK, CS ou o modulo Flash.");
}

void loop() {}
