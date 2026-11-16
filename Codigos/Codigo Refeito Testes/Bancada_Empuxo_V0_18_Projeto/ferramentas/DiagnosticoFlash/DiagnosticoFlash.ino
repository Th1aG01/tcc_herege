#include <Arduino.h>
#include <SPI.h>
#include "FlashNor.h"
FlashNor flash;
void setup() {
  Serial.begin(115200);
  pinMode(FLASH_CS, OUTPUT); digitalWrite(FLASH_CS, HIGH);
  pinMode(SPI_CS_RADIO, OUTPUT); digitalWrite(SPI_CS_RADIO, HIGH);
  SPI.begin(SPI_SCK_BANCADA, SPI_MISO_BANCADA, SPI_MOSI_BANCADA, FLASH_CS);
  flash.escrita = false;
  bool ok = flash.begin();
  Serial.printf("JEDEC: %06lX; %s\n", (unsigned long)flash.jedec, ok ? "COMPATIVEL" : flash.erro);
  Serial.println("Nenhum dado foi escrito ou apagado. Nao confirma persistencia nem identificacao da variante.");
}
void loop() {}
