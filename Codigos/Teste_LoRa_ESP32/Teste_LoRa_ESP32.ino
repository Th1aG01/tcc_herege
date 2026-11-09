/*
  Teste_LoRa_ESP32 - teste do transmissor SX1276, sem receptor.
  Arduino IDE 2 / ESP32 DevKit / biblioteca "LoRa" de Sandeep Mistry.
  Pinagem copiada do esquema enviado em 06/11/2026:
    SCK=18, MISO=19, MOSI=23, NSS/CS=21, NRESET=14, DIO0=26.
    VCC=3V3; GND comum. Conecte uma antena adequada antes de transmitir.

  Monitor Serial: 115200 baud e "Nova linha".
  1. Digite 868 ou 915 SOMENTE conforme a variante do seu modulo.
     Se a frequencia for desconhecida, confira a etiqueta/documentacao.
  2. Digite T para enviar um pacote. Repita T para outro pacote.
  3. Reinicie o ESP32 para selecionar outra frequencia.

  TxDone confirma o fim da transmissao informado pelo radio.
  Nao comprova sinal RF na antena, alcance ou recepcao por outro aparelho.
  Este sketch usa apenas o LoRa; nao e o firmware completo da bancada.
*/

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

constexpr int PIN_SCK = 18;
constexpr int PIN_MISO = 19;
constexpr int PIN_MOSI = 23;
constexpr int PIN_CS = 21;
constexpr int PIN_RST = 14;
constexpr int PIN_DIO0 = 26;
constexpr int PIN_FLASH_CS = 13;
constexpr unsigned long TIMEOUT_TX_MS = 5000;

volatile bool txConcluido = false;
bool radioPronto = false;
bool transmitindo = false;
bool falhaFatal = false;
unsigned long inicioTx = 0;
unsigned long numeroPacote = 0;

// Executada por interrupcao: apenas sinaliza; nao imprime na Serial.
void IRAM_ATTR aoConcluirTransmissao() {
  txConcluido = true;
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  delay(1000);

  // Evita que a Flash responda no SPI, caso tambem esteja conectada.
  digitalWrite(PIN_FLASH_CS, HIGH);
  pinMode(PIN_FLASH_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_CS, OUTPUT);

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  LoRa.setPins(PIN_CS, PIN_RST, PIN_DIO0);
  LoRa.setSPIFrequency(1000000);  // SPI a 1 MHz para a protoboard.
  LoRa.setSyncWord(145);          // Senha para comunicacao com o outro hardware

  Serial.println("\nTESTE LoRa SX1276");
  Serial.println("Antena conectada? Confira antes de transmitir.");
  Serial.println("Digite 868 ou 915 conforme o SEU modulo e envie com Nova linha.");
  Serial.println("Nenhum pacote sera enviado ate voce selecionar a frequencia e enviar T.");
}

void loop() {
  if (falhaFatal) {
    delay(10);
    return;
  }

  if (transmitindo) {
    if (txConcluido) {
      transmitindo = false;
      txConcluido = false;
      Serial.print("TRANSMISSAO CONCLUIDA: pacote ");
      Serial.print(numeroPacote);
      Serial.println(" (TxDone confirmado pelo radio).");
      Serial.println("Digite T para outro envio.");
    } else if (millis() - inicioTx >= TIMEOUT_TX_MS) {
      LoRa.idle();
      transmitindo = false;
      falhaFatal = true;
      Serial.println("ERRO: TxDone nao chegou em 5 segundos.");
      Serial.println("Confira DIO0/GPIO26, alimentacao e fios. Reinicie o ESP32.");
    }
    return;
  }

  if (!Serial.available()) {
    delay(1);
    return;
  }

  String comando = Serial.readStringUntil('\n');
  comando.trim();
  if (comando.length() == 0) return;

  if (!radioPronto) {
    if (comando != "868" && comando != "915") {
      Serial.println("Informe 868 ou 915 conforme o modulo; nao escolha ao acaso.");
      return;
    }

    long frequenciaHz = (comando == "915") ? 915000000L : 868000000L;
    if (!LoRa.begin(frequenciaHz)) {
      falhaFatal = true;
      Serial.println("ERRO: SX1276 nao respondeu corretamente pelo SPI.");
      Serial.println("Confira 3V3, GND, CS=21, RST=14, SCK=18, MISO=19 e MOSI=23.");
      Serial.println("Apos conferir, reinicie o ESP32.");
      return;
    }

    LoRa.setTxPower(2);           // Potencia baixa para teste de bancada.
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125000);
    LoRa.setCodingRate4(5);
    LoRa.enableCrc();
    LoRa.onTxDone(aoConcluirTransmissao);
    radioPronto = true;

    Serial.print("LoRa inicializado em ");
    Serial.print(comando);
    Serial.println(" MHz. Digite T para transmitir.");
    return;
  }

  if (comando != "T" && comando != "t") {
    Serial.println("Comando esperado: T. Para mudar a frequencia, reinicie.");
    return;
  }

  if (!LoRa.beginPacket()) {
    Serial.println("ERRO: radio indisponivel para iniciar pacote. Reinicie e confira as ligacoes.");
    return;
  }

  txConcluido = false;

  Serial.println("Enviando Mensagem " + String(numeroPacote));

  String mensagem = "Funciona";
  
  LoRa.beginPacket();
  inicioTx = millis();
  LoRa.print(mensagem + " " + String(numeroPacote));
  LoRa.endPacket();
  transmitindo = true;
  LoRa.endPacket(true);

  delay(1000);

  numeroPacote++;
}

