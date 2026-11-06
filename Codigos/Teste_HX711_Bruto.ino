/*
  TCC - Teste isolado do HX711 com ESP32 DevKit V1.
  Biblioteca: HX711 Arduino Library, de Bogdan Necula / bogde.
  DT -> GPIO33; CLK -> GPIO32; VCC -> 3V3; GND comum.
  Canal A, ganho 128. Nao faz tara, calibracao, LoRa ou acesso a flash.
  As leituras sao CONTAGENS BRUTAS, nao N, kg ou kgf.
  Monitor Serial: 115200 baud.
  A taxa depende do hardware RATE do modulo; este codigo nao a altera.
  Ao enviar este sketch, o firmware anterior do ESP32 sera substituido.
*/
#include <HX711.h>

constexpr uint8_t HX_DT = 33;
constexpr uint8_t HX_CLK = 32;
HX711 celula;
unsigned long ultimaLeituraMs = 0;
unsigned long ultimoAvisoMs = 0;
unsigned long tempoAnteriorUs = 0;
unsigned long sequencia = 0;
bool primeiraLeitura = true;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("# Teste HX711: DT=33, CLK=32, canal A, ganho 128");
  Serial.println("# Valores brutos; sem tara ou calibracao.");
  celula.begin(HX_DT, HX_CLK, 128);
  ultimaLeituraMs = millis();
  ultimoAvisoMs = ultimaLeituraMs;
  Serial.println("seq;tempo_us;intervalo_us;bruto;estado");
}

void loop() {
  if (celula.is_ready()) {
    const long bruto = celula.read();
    const unsigned long agoraUs = micros();
    const unsigned long intervaloUs = primeiraLeitura ? 0 : agoraUs - tempoAnteriorUs;
    primeiraLeitura = false;
    tempoAnteriorUs = agoraUs;
    ultimaLeituraMs = millis();

    Serial.print(sequencia++);
    Serial.print(';');
    Serial.print(agoraUs);
    Serial.print(';');
    Serial.print(intervaloUs);
    Serial.print(';');
    Serial.print(bruto);
    Serial.print(';');
    Serial.println((bruto == 8388607L || bruto == -8388608L)
                   ? "LIMITE_ADC" : "LEITURA");
  } else {
    const unsigned long agoraMs = millis();
    if (agoraMs - ultimaLeituraMs >= 2000 && agoraMs - ultimoAvisoMs >= 2000) {
      Serial.println("# SEM DADOS: confira alimentacao, GND, DT e CLK.");
      ultimoAvisoMs = agoraMs;
    }
    delay(1);
  }
}
