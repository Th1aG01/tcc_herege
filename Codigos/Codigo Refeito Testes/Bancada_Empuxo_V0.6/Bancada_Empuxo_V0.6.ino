#include <HX711.h>
#include <SPI.h>
#include <LoRa.h>

const int PINO_DT = 33;
const int PINO_SCK = 32;
const int LORA_CS = 21;
const int LORA_RST = 14;
const int LORA_DIO0 = 26;
const int LED_EXEC = 4; // Verde
const int LED_LORA = 2; // Amarelo
const unsigned long INTERVALO_ENVIO_MS = 250; // 4 envios por segundo

HX711 celula;
bool loraPronto = false;
bool ensaioAtivo = false;
bool transmitirLoRa = false;
unsigned long inicioEnsaio = 0;
unsigned long ultimoEnvio = 0;

// Ultima leitura valida da celula (o envio LoRa usa sempre esta leitura,
// assim o intervalo de 250 ms nao depende da taxa de amostragem do HX711).
long ultimaLeitura = 0;
unsigned long ultimoTempoLeitura = 0;
bool temLeitura = false;

void setup() {
  Serial.begin(115200);

  pinMode(LED_EXEC, OUTPUT);
  pinMode(LED_LORA, OUTPUT);
  digitalWrite(LED_EXEC, LOW);
  digitalWrite(LED_LORA, LOW);

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
  Serial.println("Comandos: i=iniciar; f=finalizar; l=ligar/desligar envio LoRa.");
}

void loop() {
  ReceberComando();
  digitalWrite(LED_EXEC, ensaioAtivo);
  digitalWrite(LED_LORA, ensaioAtivo && transmitirLoRa);

  // Leitura da celula (so quando o HX711 tem um dado novo).
  if (celula.is_ready()) {
    long leitura = celula.get_value();

    if (ensaioAtivo) {
      unsigned long tempo = millis() - inicioEnsaio;
      ultimaLeitura = leitura;
      ultimoTempoLeitura = tempo;
      temLeitura = true;

      Serial.print(tempo);
      Serial.print(';');
      Serial.println(leitura);
    }
  }

  // Envio LoRa: a cada 250 ms enquanto o envio estiver ligado.
  // Fica fora do bloco do HX711 para nao depender de quando a celula fica pronta.
  if (ensaioAtivo && loraPronto && transmitirLoRa && temLeitura) {
    unsigned long agora = millis();

    if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
      if (LoRa.beginPacket()) {
        LoRa.print(ultimoTempoLeitura);
        LoRa.print(';');
        LoRa.print(ultimaLeitura);
        LoRa.endPacket(true); // Continua lendo enquanto o radio transmite.

        // Mantem o ritmo exato de 4 envios/s (sem acumular atraso).
        ultimoEnvio += INTERVALO_ENVIO_MS;
        // Se ficou muito para tras, ressincroniza.
        if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) ultimoEnvio = agora;
      }
    }
  }
}

void ReceberComando() {
  if (!Serial.available()) return;
  char comando = Serial.read();

  if (comando == 'i' && !ensaioAtivo) {
    inicioEnsaio = millis();
    ultimoEnvio = inicioEnsaio;
    transmitirLoRa = false;
    temLeitura = false;
    ensaioAtivo = true;
    Serial.println("tempo_ms;leitura_bruta");
  } else if (comando == 'f' && ensaioAtivo) {
    ensaioAtivo = false;
    transmitirLoRa = false;
    Serial.println("Ensaio encerrado.");
  } else if (comando == 'l') {
    if (!ensaioAtivo) {
      Serial.println("Inicie o ensaio com i.");
    } else if (!loraPronto) {
      Serial.println("LoRa indisponivel.");
    } else {
      transmitirLoRa = !transmitirLoRa;
      if (transmitirLoRa) {
        // Primeiro envio imediato; depois segue a cada 250 ms.
        ultimoEnvio = millis() - INTERVALO_ENVIO_MS;
      }
      Serial.println(transmitirLoRa ? "Envio LoRa ligado." : "Envio LoRa desligado.");
    }
  }
}
