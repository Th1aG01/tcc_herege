#include <cassert>
#include <iostream>
#include "../../firmware/Bancada_TCC_v0_3_1/Bancada_TCC_v0_3_1.ino"

unsigned sessionNumber = 0;

void beginTest(uint32_t seconds = 30) {
  assert(!recording);
  mockWriteFails = false;
  mockUsedBytes = 1000;
  mockReadDelay = 0;
  cell.ready = false;
  hasSample = fsReady = true;
  latestRaw = 0;
  lastRead = millis();
  Serial.output.clear();
  char sid[17];
  snprintf(sid, sizeof(sid), "%016x", ++sessionNumber);
  startRecording(sid, seconds, true);
  assert(recording && count == 0 && latestUs == 0 && stoppedElapsedUs == 0);
  assert(Serial.output.find(";RECORDING;0;" + std::to_string(seconds * 1000000ULL)) != std::string::npos);
}

void normalTest(uint32_t period, uint64_t initial, bool radio) {
  mockTime = initial;
  beginTest();
  radioReady = radioEnabled = radio;
  const uint64_t origin = mockTime;
  uint64_t nextSample = origin;
  while (recording) {
    if (mockTime >= nextSample) {
      cell.ready = true;
      cell.raw = (count % 3 == 0 ? 0 : (count % 3 == 1 ? -20000 : 100000));
      nextSample += period;
    }
    if (txBusy) onRadioTxDone();
    loop();
    assert(mockTime - origin < 31000);
  }
  if (txBusy) { onRadioTxDone(); serviceRadio(); }
  assert(std::string(lastReason) == "LIMIT");
  assert(stoppedElapsedUs == 30000000);
  assert(latestUs < 30000000);
  assert(count == (30000 + period - 1) / period);
  assert(Serial.output.find(";STOPPED;30000000;30000000") != std::string::npos);
  auto frozen = stoppedElapsedUs;
  mockTime += 1500;
  char commandText[40]; snprintf(commandText, sizeof(commandText), "STOP %s", session);
  handleCommand(commandText);
  assert(stoppedElapsedUs == frozen);
  std::cout << "30 s: intervalo " << period << " ms, LoRa=" << radio << ", amostras=" << count << " OK\n";
}

int main() {
  // Taxas diferentes, valores zero/negativos/positivos e partidas consecutivas.
  normalTest(95, 10000, false);
  normalTest(13, mockTime, true);
  normalTest(100, mockTime, false);
  // millis() transborda durante o ensaio; esp_timer mantém o tempo de 64 bits.
  normalTest(95, (1ULL << 32) - 1500, false);

  // Intervalos irregulares não se tornam o relógio da duração.
  beginTest();
  uint64_t origin = mockTime, nextSample = origin;
  while (recording) {
    if (mockTime >= nextSample) {
      cell.ready = true;
      nextSample += count % 2 ? 17 : 190;
    }
    loop();
  }
  assert(std::string(lastReason) == "LIMIT" && stoppedElapsedUs == 30000000);
  std::cout << "30 s com taxa irregular OK\n";

  // Falta de dados é interrupção, não sucesso nem amostra artificial zero.
  for (uint32_t lastSampleMs : {1000, 5000, 10000}) {
    beginTest();
    origin = mockTime;
    mockTime += lastSampleMs;
    cell.ready = true;
    serviceSensor();
    assert(count == 1);
    mockTime += 2001;
    serviceSensor();
    assert(!recording && std::string(lastReason) == "HX711_TIMEOUT");
    assert(stoppedElapsedUs == uint64_t(lastSampleMs + 2001) * 1000 && count == 1);
  }
  std::cout << "Interrupções por HX711 em 3/7/12 s identificadas OK\n";

  // Uma leitura que cruza o limite não pode entrar depois do prazo.
  beginTest();
  mockTime += 29999;
  cell.ready = true;
  mockReadDelay = 2;
  serviceSensor();
  assert(!recording && std::string(lastReason) == "LIMIT");
  assert(count == 0 && stoppedElapsedUs == 30001000);

  beginTest();
  mockTime += 3000;
  stopRecording("MANUAL");
  assert(stoppedElapsedUs == 3000000 && std::string(lastReason) == "MANUAL");

  beginTest();
  mockTime += 300;
  cell.ready = true;
  mockWriteFails = true;
  serviceSensor();
  assert(!recording && std::string(lastReason) == "FLASH_WRITE" && stoppedElapsedUs == 300000);

  beginTest();
  mockTime += 1000;
  cell.ready = true;
  mockUsedBytes = 2000000 - FLASH_RESERVE + 1;
  serviceSensor();
  assert(!recording && std::string(lastReason) == "FLASH_FULL" && stoppedElapsedUs == 1000000);
  std::cout << "Limite durante leitura, parada manual, erro de flash e memória cheia OK\n";
}
