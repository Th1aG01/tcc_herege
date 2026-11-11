#include <HX711.h>
#include <SPI.h>
#include <LoRa.h>

const int PINO_DT = 33;
const int PINO_SCK = 32;
const int LORA_CS = 21;
const int LORA_RST = 14;
const int LORA_DIO0 = 26;
const int FLASH_CS = 13;
const unsigned long INTERVALO_ENVIO_MS = 500;

HX711 celula;
bool loraPronto = false;
unsigned long ultimoEnvio = 0;

void setup() {
  Serial.begin(115200);

  // Mantem a Flash desmarcada no barramento SPI compartilhado.
  digitalWrite(FLASH_CS, HIGH);
  pinMode(FLASH_CS, OUTPUT);

  SPI.begin(18, 19, 23, LORA_CS); // SCK, MISO, MOSI, CS
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  LoRa.setSPIFrequency(1000000);
  loraPronto = LoRa.begin(915E6);

  if (loraPronto) {
    LoRa.setSyncWord(145);
    LoRa.setTxPower(2);
  }
  Serial.println(loraPronto ? "LoRa pronto." : "Falha ao iniciar LoRa.");

  celula.begin(PINO_DT, PINO_SCK);
  Serial.println("Zerando a celula...");
  celula.tare();
  Serial.println("Celula pronta.");
}

void loop() {
  if (celula.is_ready()) {
    long leitura = celula.get_value();
    Serial.println(leitura);

    if (loraPronto && millis() - ultimoEnvio >= INTERVALO_ENVIO_MS) {
      if (LoRa.beginPacket()) {
        LoRa.print(leitura);
        LoRa.endPacket(true); // Continua lendo enquanto o radio transmite.
        ultimoEnvio = millis();
      }
    }
  }
}
