/*
 * TCC BRAVO - Diagnostico GPIO13/GPIO19 bidirecional - V0.18
 * Diagnostico APENAS dos pinos do ESP32, sem SPI e sem acessar memorias.
 * 
 * SEGURANCA: Com ESP32 DESENERGIZADO, desconectar Flash, LoRa e
 * manter ignicao FISICAMENTE desabilitada. Retirar todos os outros
 * fios dos GPIO13 e GPIO19. Instalar UM unico jumper entre GPIO13
 * e GPIO19. NUNCA conectar o jumper a 3V3 ou GND.
 * 
 * Monitor Serial 115200: enviar T para testar.
 * As leituras com resistores internos de pull-up/pull-down opostos
 * ajudam a identificar entrada flutuante ou ligacao interrompida.
 */
#include <Arduino.h>

static constexpr uint8_t PIN_CS = 13;
static constexpr uint8_t PIN_MISO = 19;
static constexpr unsigned CICLOS = 80;

static void testarDirecao(uint8_t saida, uint8_t entrada) {
  // Evitar que dois GPIOs sejam saidas ao mesmo tempo.
  pinMode(PIN_CS, INPUT);
  pinMode(PIN_MISO, INPUT);
  digitalWrite(saida, LOW);
  pinMode(saida, OUTPUT);

  unsigned alto = 0;
  unsigned baixo = 0;
  for (unsigned i = 0; i < CICLOS; ++i) {
    // Polarizacao contraria ao nivel enviado para detectar fio aberto.
    pinMode(entrada, INPUT_PULLDOWN);
    digitalWrite(saida, HIGH);
    delayMicroseconds(500);
    if (digitalRead(entrada) == HIGH) ++alto;

    pinMode(entrada, INPUT_PULLUP);
    digitalWrite(saida, LOW);
    delayMicroseconds(500);
    if (digitalRead(entrada) == LOW) ++baixo;
  }

  pinMode(saida, INPUT);
  pinMode(entrada, INPUT);
  Serial.printf("GPIO%u -> GPIO%u: alto=%u/%u, baixo=%u/%u - %s\n",
                saida, entrada, alto, CICLOS, baixo, CICLOS,
                (alto == CICLOS && baixo == CICLOS) ? "PASSOU" : "FALHOU");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_CS, INPUT);
  pinMode(PIN_MISO, INPUT);
  delay(500);
  Serial.println("=== TCC BRAVO - GPIO13/GPIO19 BIDIRECIONAL V0.18 ===");
  Serial.println("Flash e LoRa totalmente desconectados; ignicao desabilitada.");
  Serial.println("UM jumper direto entre GPIO13 e GPIO19 (sem outros fios).");
  Serial.println("Enviar T e Enter para executar as duas direcoes.");
}

void loop() {
  if (Serial.available() <= 0) return;
  const char c = static_cast<char>(Serial.read());
  if (c != 'T' && c != 't') return;
  testarDirecao(PIN_CS, PIN_MISO);
  testarDirecao(PIN_MISO, PIN_CS);
  Serial.println("Fim. Nenhuma operacao SPI, escrita ou apagamento de memoria.");
}
