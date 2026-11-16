#pragma once
// Sem onTxDone: LoRa 0.8.0 acessa SPI antes de chamar o callback da aplicacao.
// Consultar IRQ_FLAGS do SX127x no loop evita SPI concorrente com a Flash.
inline bool radioTxConcluido() {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SPI_CS_RADIO, LOW);
  SPI.transfer(0x12); uint8_t flags = SPI.transfer(0);
  digitalWrite(SPI_CS_RADIO, HIGH); SPI.endTransaction();
  if (!(flags & 0x08) || flags == 0xff) return false;
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SPI_CS_RADIO, LOW);
  SPI.transfer(0x92); SPI.transfer(0x08);
  digitalWrite(SPI_CS_RADIO, HIGH); SPI.endTransaction();
  return true;
}
