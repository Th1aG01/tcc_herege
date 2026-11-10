/* TCC - v0.3.1: HX711 bruto + LoRa BRAVO + USB + LittleFS.
   NÃO há saída de ignição nem conversão para kg/N neste programa.
   Bibliotecas: LoRa 0.8.0 (Sandeep Mistry), HX711 (Bogdan Necula/bogde).
   Placa: DOIT ESP32 DEVKIT V1. Monitor serial: 115200, Nova linha.
*/
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <HX711.h>
#include <LittleFS.h>
#include <esp_timer.h>
#include <mbedtls/base64.h>

constexpr int HX_DT = 33, HX_CLK = 32;
constexpr int LORA_SCK = 18, LORA_MISO = 19, LORA_MOSI = 23;
constexpr int LORA_CS = 21, LORA_RST = 14, LORA_DIO0 = 26;
constexpr int FUTURE_FLASH_CS = 13;
constexpr long LORA_HZ = 915000000L;
constexpr uint8_t LORA_SYNC = 145;  // 0x91: BRAVO
constexpr uint32_t RADIO_PERIOD_MS = 500, TX_TIMEOUT_MS = 2000;
constexpr uint32_t HX_TIMEOUT_MS = 2000, FLASH_RESERVE = 16384;
constexpr uint32_t CLOCK_PERIOD_MS = 250;
constexpr size_t FILE_BLOCK_SIZE = 120;
constexpr uint32_t FILE_PACKET_INTERVAL_MS = 400;
constexpr uint32_t MAX_RADIO_FILE = 16UL * 1024UL * 1024UL;

HX711 cell;
File logFile, dumpFile, radioFile;
bool fsReady = false, radioReady = false, radioEnabled = false, txBusy = false;
bool fileSending = false, txForFile = false;
char fileSession[17] = "", txFileKind = 'S';
uint32_t radioFileSize = 0, radioFileBlocks = 0, radioFileCrc = 0;
uint32_t radioFileNext = 0, radioFileDone = 0, txFileIndex = 0;
uint8_t radioFileStage = 0, radioFileRepeats = 0;
volatile bool txDone = false;
bool recording = false, hasSample = false, useFlash = false;
char session[17] = "", lastReason[24] = "", dumpSession[17] = "";
uint32_t count = 0, maxDuration = 120, txNext = 0, txActive = 0;
uint32_t txStarted = 0, txLast = 0, lastRead = 0, lastPreview = 0, lastFlush = 0;
uint32_t latestSeq = 0, lastSentSeq = 0, dumpBytes = 0;
bool sentAny = false;
int32_t latestRaw = 0;
uint64_t startedUs = 0, latestUs = 0, stoppedElapsedUs = 0;
uint32_t lastClockMs = 0;
char command[120];
size_t commandLen = 0;
bool commandOverflow = false;

void IRAM_ATTR onRadioTxDone() { txDone = true; }

uint16_t crc16(const char *s) {
  uint16_t crc = 0xffff;
  while (*s) {
    crc ^= uint16_t(uint8_t(*s++)) << 8;
    for (int i = 0; i < 8; ++i) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
  }
  return crc;
}

const char *flagFor(int32_t raw) {
  return (raw == -8388608 || raw == 8388607) ? "SAT" : "OK";
}

bool validSession(const char *s) {
  if (strlen(s) != 16) return false;
  for (int i = 0; i < 16; ++i)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
  return true;
}

void filePath(const char *sid, char *out, size_t size) {
  snprintf(out, size, "/tcc_%s.log", sid);
}

size_t freeBytes() {
  return fsReady ? LittleFS.totalBytes() - LittleFS.usedBytes() : 0;
}

void error(const char *reason) { Serial.printf("ERR;%s\n", reason); }

void sendClock() {
  if (!session[0]) return;
  const uint64_t elapsed = recording ? uint64_t(esp_timer_get_time()) - startedUs : stoppedElapsedUs;
  Serial.printf("CLOCK;%s;%s;%llu;%llu\n", session, recording ? "RECORDING" : "STOPPED",
    (unsigned long long)elapsed, (unsigned long long)maxDuration * 1000000ULL);
  lastClockMs = millis();
}

void status() {
  const char *hx = !hasSample || uint32_t(millis() - lastRead) > HX_TIMEOUT_MS ? "WAIT" : flagFor(latestRaw);
  Serial.printf("STATUS;%s;%s;%lu;%s;%lu;%s;%s\n",
    recording ? "RECORDING" : (fileSending ? "TRANSFER" : "IDLE"), session[0] ? session : "-", (unsigned long)count,
    fsReady ? "OK" : "UNAVAILABLE", (unsigned long)freeBytes(),
    !radioReady ? "FAIL" : (fileSending ? "FILE_TX" : (radioEnabled ? "ON" : "OFF")), hx);
  sendClock();
}

void encodeSample(char *out, size_t size, bool radio) {
  char body[160], packet[16];
  if (radio) snprintf(packet, sizeof(packet), "%lu", (unsigned long)txNext);
  else strcpy(packet, "-");
  snprintf(body, sizeof(body), "TCC2;%s;%s;%lu;%llu;%ld;%s", session, packet,
           (unsigned long)latestSeq, (unsigned long long)latestUs,
           (long)latestRaw, flagFor(latestRaw));
  snprintf(out, size, "%s;%04X", body, (unsigned int)crc16(body));
}

bool writeLine(File &file, const char *line) {
  const size_t n = strlen(line);
  return file.write((const uint8_t *)line, n) == n && file.write((uint8_t)'\n') == 1;
}

void stopRecording(const char *reason) {
  if (!recording) return;
  // Congela o tempo ANTES de fechar a flash ou transmitir a confirmação.
  stoppedElapsedUs = uint64_t(esp_timer_get_time()) - startedUs;
  recording = false;
  strncpy(lastReason, reason, sizeof(lastReason) - 1);
  lastReason[sizeof(lastReason) - 1] = 0;
  if (logFile) {
    char line[96];
    snprintf(line, sizeof(line), "END;%s;%s;%lu", session, reason, (unsigned long)count);
    if (!writeLine(logFile, line)) {
      strcpy(lastReason, "FLASH_WRITE");
      error("FLASH_WRITE");
    }
    logFile.flush();
    logFile.close();
  }
  sendClock();
  Serial.printf("ACK;STOP;%s;%s;%lu\n", session, lastReason, (unsigned long)count);
}

void serviceTiming() {
  if (!recording) return;
  if (uint64_t(esp_timer_get_time()) - startedUs >= uint64_t(maxDuration) * 1000000ULL)
    stopRecording("LIMIT");
  else if (uint32_t(millis() - lastClockMs) >= CLOCK_PERIOD_MS) sendClock();
}

void startRecording(const char *sid, uint32_t duration, bool flash) {
  if (!validSession(sid) || duration < 5 || duration > 3600) { error("START_ARGUMENTS"); return; }
  if (dumpFile) { error("DOWNLOAD_ACTIVE"); return; }
  if (fileSending) { error("RADIO_FILE_ACTIVE"); return; }
  if (recording) {
    if (!strcmp(sid, session)) Serial.printf("ACK;START;%s\n", session);
    else error("BUSY");
    return;
  }
  if (txBusy) { error("RADIO_FINISHING"); return; }
  if (!hasSample || uint32_t(millis() - lastRead) > HX_TIMEOUT_MS) { error("HX711_NO_DATA"); return; }
  if (!strcmp(flagFor(latestRaw), "SAT")) { error("HX711_SATURATED"); return; }
  if (flash) {
    if (!fsReady) { error("FLASH_NOT_READY"); return; }
    if (freeBytes() < FLASH_RESERVE * 2) { error("FLASH_FULL"); return; }
    char path[40];
    filePath(sid, path, sizeof(path));
    if (LittleFS.exists(path)) { error("SESSION_EXISTS"); return; }
    logFile = LittleFS.open(path, FILE_WRITE);
    if (!logFile) { error("FLASH_OPEN"); return; }
    char line[48];
    snprintf(line, sizeof(line), "META;TCC2;%s", sid);
    if (!writeLine(logFile, line)) { logFile.close(); error("FLASH_WRITE"); return; }
    logFile.flush();
  }
  strcpy(session, sid);
  lastReason[0] = 0;
  count = txNext = 0;
  latestSeq = 0;
  latestUs = stoppedElapsedUs = 0;
  sentAny = false;
  maxDuration = duration;
  useFlash = flash;
  lastFlush = millis();
  txLast = millis() - RADIO_PERIOD_MS;
  startedUs = (uint64_t)esp_timer_get_time();
  recording = true;
  Serial.printf("ACK;START;%s\n", session);
  status();
}

uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
  }
  return crc;
}

void cancelRadioFile(const char *reason) {
  if (!fileSending) return;
  fileSending = false;
  if (radioFile) radioFile.close();
  // Um pacote já no ar pode terminar. Nenhum novo bloco será iniciado.
  Serial.printf("LORA_FILE;CANCELLED;%s;%s\n", fileSession, reason);
}

void beginRadioFile(const char *sid) {
  if (!validSession(sid)) { error("LORA_SEND_ARGUMENTS"); return; }
  if (recording || fileSending || txBusy || dumpFile) { error("BUSY"); return; }
  if (!fsReady) { error("FLASH_NOT_READY"); return; }
  if (!radioReady) { error("LORA_NOT_READY"); return; }
  char path[40]; filePath(sid, path, sizeof(path));
  radioFile = LittleFS.open(path, FILE_READ);
  if (!radioFile) { error("FILE_NOT_FOUND"); return; }
  radioFileSize = radioFile.size();
  if (!radioFileSize || radioFileSize > MAX_RADIO_FILE) {
    radioFile.close(); error("RADIO_FILE_SIZE"); return;
  }
  // Lê o registro já encerrado para calcular a integridade do arquivo inteiro.
  uint32_t crc = 0xffffffff, totalRead = 0;
  uint8_t buffer[512];
  while (radioFile.available()) {
    size_t n = radioFile.read(buffer, sizeof(buffer));
    if (!n) break;
    crc = crc32Update(crc, buffer, n);
    totalRead += n;
    yield();
  }
  if (totalRead != radioFileSize || !radioFile.seek(0)) {
    radioFile.close(); error("RADIO_FILE_READ"); return;
  }
  strcpy(fileSession, sid);
  radioFileCrc = crc ^ 0xffffffff;
  radioFileBlocks = (radioFileSize + FILE_BLOCK_SIZE - 1) / FILE_BLOCK_SIZE;
  radioFileNext = radioFileDone = 0;
  radioFileStage = radioFileRepeats = 0;
  fileSending = true;
  txLast = millis() - FILE_PACKET_INTERVAL_MS;
  Serial.printf("LORA_FILE;START;%s;%lu;%lu;%08lX\n", fileSession,
    (unsigned long)radioFileBlocks, (unsigned long)radioFileSize, (unsigned long)radioFileCrc);
  status();
}

void sendNextFilePacket() {
  uint8_t raw[FILE_BLOCK_SIZE];
  unsigned char encoded[4 * ((FILE_BLOCK_SIZE + 2) / 3) + 1] = {0};
  char body[244], line[250];
  char kind = radioFileStage == 0 ? 'S' : (radioFileStage == 1 ? 'B' : 'E');
  uint32_t index = kind == 'B' ? radioFileNext : 0;
  if (kind == 'B') {
    const size_t expected = min((uint32_t)FILE_BLOCK_SIZE, radioFileSize - index * FILE_BLOCK_SIZE);
    if (!radioFile.seek(index * FILE_BLOCK_SIZE) || radioFile.read(raw, expected) != expected) {
      cancelRadioFile("READ_ERROR"); return;
    }
    size_t output = 0;
    if (mbedtls_base64_encode(encoded, sizeof(encoded), &output, raw, expected) != 0) {
      cancelRadioFile("ENCODING_ERROR"); return;
    }
    encoded[output] = 0;
  }
  snprintf(body, sizeof(body), "TCCF3;%s;%c;%lu;%lu;%lu;%08lX;%s", fileSession, kind,
    (unsigned long)index, (unsigned long)radioFileBlocks, (unsigned long)radioFileSize,
    (unsigned long)radioFileCrc, (char *)encoded);
  const int length = snprintf(line, sizeof(line), "%s;%04X", body, (unsigned int)crc16(body));
  if (length <= 0 || length >= (int)sizeof(line)) { cancelRadioFile("PACKET_TOO_LONG"); return; }
  if (!LoRa.beginPacket()) {
    cancelRadioFile("RADIO_BUSY"); return;
  }
  txDone = false;
  if (LoRa.write((const uint8_t *)line, length) != (size_t)length || !LoRa.endPacket(true)) {
    cancelRadioFile("TX_START_ERROR"); return;
  }
  txForFile = txBusy = true;
  txFileKind = kind;
  txFileIndex = index;
  txStarted = txLast = millis();
  // Avança apenas a agenda de envio. O progresso local só confirma em TX_DONE.
  if (kind == 'S' && ++radioFileRepeats >= 2) { radioFileStage = 1; radioFileRepeats = 0; }
  else if (kind == 'B' && ++radioFileNext >= radioFileBlocks) { radioFileStage = 2; radioFileRepeats = 0; }
  else if (kind == 'E') ++radioFileRepeats;
}

void serviceRadio() {
  if (!radioReady) return;
  const uint32_t now = millis();
  if (txBusy) {
    if (txDone) {
      txDone = false;
      txBusy = false;
      if (txForFile) {
        txForFile = false;
        if (fileSending && txFileKind == 'B') {
          radioFileDone = txFileIndex + 1;
          Serial.printf("LORA_FILE;PROGRESS;%s;%lu;%lu\n", fileSession,
            (unsigned long)radioFileDone, (unsigned long)radioFileBlocks);
        }
        if (fileSending && txFileKind == 'E' && radioFileRepeats >= 3) {
          fileSending = false;
          radioFile.close();
          Serial.printf("LORA_FILE;SENT;%s;%lu\n", fileSession, (unsigned long)radioFileBlocks);
          status();  // SENT é conclusão LOCAL, não ACK do receptor.
        }
      } else Serial.printf("TX_DONE;%s;%lu\n", session, (unsigned long)txActive);
    } else if (uint32_t(now - txStarted) > TX_TIMEOUT_MS) {
      LoRa.sleep();
      txBusy = false;
      radioReady = false;
      txForFile = false;
      cancelRadioFile("TX_TIMEOUT");
      error("LORA_TX_TIMEOUT");
    }
    return;
  }
  if (fileSending) {
    if (uint32_t(now - txLast) >= FILE_PACKET_INTERVAL_MS) sendNextFilePacket();
    return;
  }
  if (!recording || !radioEnabled || !count || uint32_t(now - txLast) < RADIO_PERIOD_MS) return;
  if (sentAny && latestSeq == lastSentSeq) return;  // não retransmite dado antigo como novo
  char line[180];
  encodeSample(line, sizeof(line), true);
  if (!LoRa.beginPacket()) return;
  const size_t size = strlen(line);
  txDone = false;
  if (LoRa.write((const uint8_t *)line, size) != size || !LoRa.endPacket(true)) {
    radioReady = false;
    error("LORA_TX_START");
    return;
  }
  txActive = txNext++;
  txForFile = false;
  txBusy = true;
  txStarted = txLast = now;
  lastSentSeq = latestSeq;
  sentAny = true;
  // Esta mensagem NÃO é uma confirmação da estação receptora.
  Serial.printf("TX_BEGIN;%s;%lu;%lu\n", session, (unsigned long)txActive, (unsigned long)latestSeq);
}

void serviceSensor() {
  serviceTiming();
  if (!cell.is_ready()) {
    if (recording && uint32_t(millis() - lastRead) > HX_TIMEOUT_MS) stopRecording("HX711_TIMEOUT");
    return;
  }
  // Somente esta rotina lê o HX711. Sem média/tara/fator de escala nesta etapa.
  latestRaw = (int32_t)cell.read();
  const uint64_t stamp = (uint64_t)esp_timer_get_time();
  lastRead = millis();
  hasSample = true;
  // A leitura pode cruzar o prazo. Não inclui amostra além da janela solicitada.
  if (recording && stamp - startedUs >= uint64_t(maxDuration) * 1000000ULL)
    stopRecording("LIMIT");
  if (!recording) {
    if (uint32_t(lastRead - lastPreview) >= 100) {
      Serial.printf("PREVIEW;%llu;%ld;%s\n", (unsigned long long)stamp, (long)latestRaw, flagFor(latestRaw));
      lastPreview = lastRead;
    }
    return;
  }
  latestSeq = count++;
  latestUs = stamp - startedUs;
  char line[180];
  encodeSample(line, sizeof(line), false);
  Serial.println(line);
  if (useFlash && !writeLine(logFile, line)) { stopRecording("FLASH_WRITE"); return; }
  if (useFlash && uint32_t(lastRead - lastFlush) >= 1000) {
    logFile.flush();
    lastFlush = lastRead;
    if (freeBytes() < FLASH_RESERVE) stopRecording("FLASH_FULL");
  }
}

void listFiles() {
  if (!fsReady) { error("FLASH_NOT_READY"); return; }
  File root = LittleFS.open("/");
  File f = root.openNextFile();
  while (f) {
    const char *name = strrchr(f.name(), '/');
    name = name ? name + 1 : f.name();
    if (strlen(name) == 24 && !strncmp(name, "tcc_", 4) && !strcmp(name + 20, ".log")) {
      char sid[17]; memcpy(sid, name + 4, 16); sid[16] = 0;
      if (validSession(sid)) Serial.printf("FILE_INFO;%s;%lu\n", sid, (unsigned long)f.size());
    }
    f.close();
    f = root.openNextFile();
  }
  root.close();
  Serial.println("FILE_LIST_END");
}

void beginDownload(const char *sid) {
  if (!fsReady || !validSession(sid)) { error("GET_ARGUMENTS"); return; }
  if (dumpFile) { error("DOWNLOAD_ACTIVE"); return; }
  char path[40]; filePath(sid, path, sizeof(path));
  dumpFile = LittleFS.open(path, FILE_READ);
  if (!dumpFile) { error("FILE_NOT_FOUND"); return; }
  strcpy(dumpSession, sid);
  dumpBytes = dumpFile.size();
  Serial.printf("FILE_BEGIN;%s;%lu\n", sid, (unsigned long)dumpBytes);
}

void serviceDownload() {
  if (!dumpFile) return;
  if (dumpFile.available()) {
    char line[200];
    const size_t n = dumpFile.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = 0;
    Serial.printf("FILE_DATA;%s\n", line);
  } else {
    dumpFile.close();
    Serial.printf("FILE_END;%s;%lu\n", dumpSession, (unsigned long)dumpBytes);
  }
}

void handleCommand(char *text) {
  if (!strcmp(text, "HELLO") || !strcmp(text, "STATUS")) { status(); return; }
  if (!strcmp(text, "LORACANCEL")) { cancelRadioFile("USER"); status(); return; }
  if (fileSending) { error("RADIO_FILE_ACTIVE"); return; }
  if (!strncmp(text, "START ", 6)) {
    char sid[17], extra; unsigned long duration; int flash;
    if (sscanf(text, "START %16s %lu %d %c", sid, &duration, &flash, &extra) != 3 || (flash != 0 && flash != 1))
      error("START_ARGUMENTS");
    else startRecording(sid, duration, flash == 1);
    return;
  }
  if (!strncmp(text, "STOP ", 5)) {
    const char *sid = text + 5;
    if (!validSession(sid) || strcmp(sid, session)) { error("SESSION_MISMATCH"); return; }
    if (recording) stopRecording("MANUAL");
    else if (lastReason[0]) {
      sendClock();
      Serial.printf("ACK;STOP;%s;%s;%lu\n", session, lastReason, (unsigned long)count);
    }
    else error("NOT_RECORDING");
    return;
  }
  if (!strcmp(text, "RADIO ON") || !strcmp(text, "RADIO OFF")) {
    if (!radioReady) { error("LORA_NOT_READY"); return; }
    // Suspende NOVOS envios. O pacote já iniciado termina normalmente.
    radioEnabled = !strcmp(text, "RADIO ON");
    Serial.printf("ACK;RADIO;%s\n", radioEnabled ? "ON" : "OFF");
    return;
  }
  if (recording) { error("BUSY"); return; }
  if (dumpFile) { error("DOWNLOAD_ACTIVE"); return; }
  if (!strncmp(text, "LORASEND ", 9)) { beginRadioFile(text + 9); return; }
  if (!strcmp(text, "PREPARE_FLASH CONFIRM")) {
    // Somente comando explícito. Na inicialização normal NUNCA formata.
    if (!fsReady) fsReady = LittleFS.begin(true);
    if (!fsReady) error("FLASH_PREPARE_FAILED");
    else Serial.println("ACK;FLASH;READY");
    status();
  } else if (!strcmp(text, "LIST")) listFiles();
  else if (!strncmp(text, "GET ", 4)) beginDownload(text + 4);
  else if (!strncmp(text, "DELETE ", 7)) {
    if (!validSession(text + 7) || !fsReady) { error("DELETE_ARGUMENTS"); return; }
    char path[40]; filePath(text + 7, path, sizeof(path));
    if (LittleFS.remove(path)) Serial.printf("ACK;DELETE;%s\n", text + 7);
    else error("DELETE_FAILED");
  } else error("UNKNOWN_COMMAND");
}

void serviceCommands() {
  // Limite por volta: tráfego serial não monopoliza a aquisição.
  for (int i = 0; i < 120 && Serial.available(); ++i) {
    const char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (commandOverflow) error("COMMAND_TOO_LONG");
      else if (commandLen) { command[commandLen] = 0; handleCommand(command); }
      commandLen = 0; commandOverflow = false;
    } else if (!commandOverflow) {
      if (commandLen < sizeof(command) - 1) command[commandLen++] = c;
      else commandOverflow = true;
    }
  }
}

void setup() {
  Serial.begin(115200);
  // Mantém o futuro dispositivo SPI desmarcado; não há driver de flash externa.
  digitalWrite(FUTURE_FLASH_CS, HIGH); pinMode(FUTURE_FLASH_CS, OUTPUT);
  cell.begin(HX_DT, HX_CLK, 128);
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  LoRa.setSPIFrequency(1000000);
  radioReady = LoRa.begin(LORA_HZ);
  if (radioReady) {
    // Perfil que corresponde aos padrões SX1276 usados no exemplo BRAVO.
    LoRa.setSyncWord(LORA_SYNC);
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125000);
    LoRa.setCodingRate4(5);
    LoRa.setPreambleLength(8);
    LoRa.disableCrc();  // o pacote TCC2 inclui seu próprio CRC16 de aplicação
    LoRa.disableInvertIQ();
    LoRa.setTxPower(2);  // teste de bancada, com antena adequada conectada
    LoRa.onTxDone(onRadioTxDone);  // biblioteca 0.8.0: conclusão via DIO0 / GPIO26
  }
  fsReady = LittleFS.begin(false);  // não apaga dados ao encontrar erro de montagem
  Serial.println("EVT;READY;0.3.1");
  status();
}

void loop() {
  serviceTiming();
  serviceCommands();
  serviceSensor();
  serviceTiming();
  serviceRadio();
  serviceDownload();
  delay(1);
}
