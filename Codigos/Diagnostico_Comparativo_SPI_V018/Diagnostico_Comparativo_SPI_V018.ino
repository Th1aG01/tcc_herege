/*
 * TCC BRAVO - Diagnostico comparativo JEDEC - V0.18
 * SOMENTE LEITURA. Nenhum comando de escrita, erase ou formatacao.
 * Compara a leitura que funcionou (dummy 0x00) com o driver V0.18.
 * GPIO conforme esquema: Flash CS 13, LoRa CS 21, SCK 18, MISO 19, MOSI 23.
 * Manter ignicao fisicamente desabilitada.
 */
#include <Arduino.h>
#include <SPI.h>
#include "FlashNor.h" // copia SEM MODIFICACOES da V0.18

static uint32_t leituraDireta(uint8_t dummy) {
  SPI.beginTransaction(SPISettings(FLASH_SPI_HZ, MSBFIRST, SPI_MODE0));
  digitalWrite(SPI_CS_RADIO, HIGH); // LoRa deselecionado
  digitalWrite(FLASH_CS, LOW);
  SPI.transfer(0x9F); // comando Read JEDEC ID
  const uint8_t fabricante = SPI.transfer(dummy);
  const uint8_t tipo = SPI.transfer(dummy);
  const uint8_t capacidade = SPI.transfer(dummy);
  digitalWrite(FLASH_CS, HIGH);
  SPI.endTransaction();
  return ((uint32_t)fabricante << 16) | ((uint32_t)tipo << 8) | capacidade;
}

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_CS, OUTPUT);
  pinMode(SPI_CS_RADIO, OUTPUT);
  digitalWrite(FLASH_CS, HIGH);
  digitalWrite(SPI_CS_RADIO, HIGH);
  delay(300); // mesma espera do primeiro diagnostico bem-sucedido
  SPI.begin(SPI_SCK_BANCADA, SPI_MISO_BANCADA, SPI_MOSI_BANCADA, FLASH_CS);

  Serial.println("=== Diagnostico comparativo JEDEC V0.18 - SOMENTE LEITURA ===");
  Serial.printf("GPIO: FLASH_CS=%d LORA_CS=%d SCK=%d MISO=%d MOSI=%d\n",
                FLASH_CS, SPI_CS_RADIO, SPI_SCK_BANCADA,
                SPI_MISO_BANCADA, SPI_MOSI_BANCADA);
  // Compara envio de bytes dummy, sem mudar GPIOs nem clock.
  const uint32_t j0 = leituraDireta(0x00);
  Serial.printf("A - leitura direta dummy 00: %06lX\n", (unsigned long)j0);
  delay(30);
  const uint32_t jff = leituraDireta(0xFF);
  Serial.printf("B - leitura direta dummy FF: %06lX\n", (unsigned long)jff);
  delay(30);
  FlashNor flash;
  flash.escrita = false;
  bool ok = flash.begin();
  Serial.printf("C - driver FlashNor V018: %06lX; %s\n",
                (unsigned long)flash.jedec, ok ? "COMPATIVEL" : flash.erro);
  Serial.println("Nenhum dado foi escrito, apagado ou formatado.");
}
void loop() {}
