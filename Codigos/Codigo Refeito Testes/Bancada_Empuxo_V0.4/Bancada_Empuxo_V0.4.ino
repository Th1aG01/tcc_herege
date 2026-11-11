#include <HX711.h>
#include <SPI.h>
#include <LoRa.h>

const int PINO_DT = 33;
const int PINO_SCK = 32;
const int LORA_CS = 21;
const int LORA_RST = 14;
const int LORA_DIO0 = 26;
const unsigned long INTERVALO_ENVIO_MS = 500;

HX711 celula;
bool loraPronto = false;
bool ensaioAtivo = false;
unsigned long inicioEnsaio = 0;
unsigned long ultimoEnvio = 0;

void setup() {
  Serial.begin(115200);

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
  Serial.println("Pronto. Envie i para iniciar ou f para finalizar.");
}

void loop() {
  ReceberComando();

  if (!celula.is_ready()) return;
  long leitura = celula.get_value();
  if (!ensaioAtivo) return;

  unsigned long agora = millis();
  unsigned long tempo = agora - inicioEnsaio;
  Serial.print(tempo);
  Serial.print(';');
  Serial.println(leitura);

  if (loraPronto && agora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
    if (LoRa.beginPacket()) {
      LoRa.print(tempo);
      LoRa.print(';');
      LoRa.print(leitura);
      LoRa.endPacket(true); // Continua lendo enquanto o radio transmite.
      ultimoEnvio = agora;
    }
  }
}

void ReceberComando() {
  if (!Serial.available()) return;
  char comando = Serial.read();

  if (comando == 'i' && !ensaioAtivo) {
    inicioEnsaio = millis();
    ultimoEnvio = inicioEnsaio;
    ensaioAtivo = true;
    Serial.println("tempo_ms;leitura_bruta");
  } else if (comando == 'f' && ensaioAtivo) {
    ensaioAtivo = false;
    Serial.println("Ensaio encerrado.");
  }
}
