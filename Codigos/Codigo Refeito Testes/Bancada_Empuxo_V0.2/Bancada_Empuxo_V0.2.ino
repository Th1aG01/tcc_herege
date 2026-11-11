#include <HX711.h>

const int PINO_DT = 33;
const int PINO_SCK = 32;

HX711 celula;

void setup() {
  Serial.begin(115200);
  celula.begin(PINO_DT, PINO_SCK);
  Serial.println("Zerando a celula...");
  celula.tare();
  Serial.println("Celula pronta.");
}

void loop() {
  if (celula.is_ready()) {
    Serial.println(celula.get_value(), 0);
  }
}
