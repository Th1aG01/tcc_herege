/*
  TCC BRAVO - Diagnostico isolado GPIO SCK / CS - V0.18
  Finalidade: verificar se GPIO18 e GPIO13 alternam niveis e podem ser lidos no GPIO19.
  NAO valida a funcao SPI de hardware de SCK, a Flash ou os fios da montagem original.
  Flash e LoRa devem estar COMPLETAMENTE DESCONECTADOS; ignicao FISICAMENTE DESABILITADA.
  SOMENTE UM jumper de teste por vez, SEMPRE com o ESP32 desligado:
     S: GPIO18 (SCK) -> GPIO19 (MISO)
     C: GPIO13 (CS)  -> GPIO19 (MISO)
  Monitor serial 115200; digitar S ou C conforme o jumper instalado.
  Nao ha comandos de memoria, nem escrita, apagamento ou formatacao.
*/
#include <Arduino.h>

static constexpr int PIN_SCK = 18;
static constexpr int PIN_CS = 13;
static constexpr int PIN_ENTRADA = 19;
static constexpr unsigned CICLOS = 80;

static void testar(int saida, const char *rotulo) {
  // Somente o GPIO selecionado e alternado; o outro permanece em nivel fixo.
  digitalWrite(PIN_SCK, LOW);
  digitalWrite(PIN_CS, HIGH);
  delay(5);

  unsigned errosAlto = 0;
  unsigned errosBaixo = 0;
  for (unsigned i = 0; i < CICLOS; ++i) {
    digitalWrite(saida, HIGH);
    delayMicroseconds(200);
    if (digitalRead(PIN_ENTRADA) != HIGH) ++errosAlto;

    digitalWrite(saida, LOW);
    delayMicroseconds(200);
    if (digitalRead(PIN_ENTRADA) != LOW) ++errosBaixo;
  }

  digitalWrite(PIN_SCK, LOW);
  digitalWrite(PIN_CS, HIGH);
  Serial.printf("GPIO %s: alto=%u/%u, baixo=%u/%u - %s\n",
                rotulo,
                CICLOS - errosAlto, CICLOS,
                CICLOS - errosBaixo, CICLOS,
                (!errosAlto && !errosBaixo) ? "PASSOU" : "FALHOU");
  Serial.println("Este teste verifica saida/entrada GPIO, NAO o protocolo SPI nem o modulo Flash.");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_ENTRADA, INPUT);
  pinMode(PIN_SCK, OUTPUT);
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_SCK, LOW);
  digitalWrite(PIN_CS, HIGH);
  delay(500);
  Serial.println("=== TCC BRAVO - TESTE GPIO SCK/CS V0.18 ===");
  Serial.println("REQUISITO: Flash e LoRa desconectados; ignicao desabilitada.");
  Serial.println("Com USB desligado, instale somente UM jumper de teste:");
  Serial.println("  GPIO18 -> GPIO19 : digite S e pressione Enter");
  Serial.println("  GPIO13 -> GPIO19 : digite C e pressione Enter");
  Serial.println("Desligue o USB ANTES de mover o jumper entre S e C.");
}

void loop() {
  if (!Serial.available()) return;
  const char comando = (char)Serial.read();
  if (comando == 'S' || comando == 's') testar(PIN_SCK, "18 (SCK)");
  if (comando == 'C' || comando == 'c') testar(PIN_CS, "13 (CS)");
}
