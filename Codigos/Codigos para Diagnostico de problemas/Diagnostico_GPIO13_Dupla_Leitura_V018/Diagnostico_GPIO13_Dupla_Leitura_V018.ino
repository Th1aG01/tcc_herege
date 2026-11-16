/*
  TCC BRAVO - Diagnostico isolado GPIO13 - V0.18
  Somente leitura de GPIO; NAO acessa memorias, flash ou radio.
  Antes de energizar: desconectar COMPLETAMENTE Flash e LoRa,
  desconectar a ignicao FISICAMENTE e retirar TODOS os outros fios
  ligados a GPIO13 e GPIO19.
  Instalar UM unico jumper entre GPIO13 e GPIO19, com USB desligado.
  Monitor Serial: 115200 baud. Digitar T e Enter para testar.
  Nenhuma alteracao permanente da pinagem do firmware principal.
*/
#include <Arduino.h>

static constexpr int PIN_SAIDA = 13;
static constexpr int PIN_ENTRADA = 19;
static constexpr unsigned CICLOS = 80;

static void executar() {
  unsigned proprioAlto = 0, proprioBaixo = 0;
  unsigned externoAlto = 0, externoBaixo = 0;
  for (unsigned i = 0; i < CICLOS; ++i) {
    digitalWrite(PIN_SAIDA, HIGH);
    delayMicroseconds(250);
    proprioAlto += (digitalRead(PIN_SAIDA) == HIGH);
    externoAlto += (digitalRead(PIN_ENTRADA) == HIGH);

    digitalWrite(PIN_SAIDA, LOW);
    delayMicroseconds(250);
    proprioBaixo += (digitalRead(PIN_SAIDA) == LOW);
    externoBaixo += (digitalRead(PIN_ENTRADA) == LOW);
  }
  digitalWrite(PIN_SAIDA, HIGH); // CS inativo apos o teste
  Serial.printf("GPIO13 leitura propria: alto=%u/80, baixo=%u/80 - %s\n",
                proprioAlto, proprioBaixo,
                (proprioAlto == CICLOS && proprioBaixo == CICLOS) ? "PASSOU" : "FALHOU");
  Serial.printf("GPIO19 via jumper:     alto=%u/80, baixo=%u/80 - %s\n",
                externoAlto, externoBaixo,
                (externoAlto == CICLOS && externoBaixo == CICLOS) ? "PASSOU" : "FALHOU");
  Serial.println("A leitura propria nao substitui uma medicao eletrica no pino.");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_ENTRADA, INPUT);
  pinMode(PIN_SAIDA, OUTPUT);
  digitalWrite(PIN_SAIDA, HIGH);
  delay(500);
  Serial.println("=== TCC BRAVO - GPIO13 dupla leitura V0.18 ===");
  Serial.println("Flash e LoRa desconectados; ignicao fisicamente desabilitada.");
  Serial.println("UM jumper GPIO13 --> GPIO19; nenhuma outra ligacao nesses pinos.");
  Serial.println("Envie T e pressione Enter para testar.");
}

void loop() {
  if (Serial.available()) {
    const char comando = static_cast<char>(Serial.read());
    if (comando == 'T' || comando == 't') executar();
  }
}
