#include <HX711.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h> // ArduinoJson 7.x (Gerenciador de Bibliotecas)

const int PINO_DT = 33;
const int PINO_SCK = 32;
const int LORA_CS = 21;
const int LORA_RST = 14;
const int LORA_DIO0 = 26;
const int LED_EXEC = 4; // Verde
const int LED_LORA = 2; // Amarelo
const unsigned long INTERVALO_ENVIO_MS = 250; // 4 envios por segundo

// >>> AJUSTE AQUI: fator de calibracao da celula de carga <<<
// empuxo = leitura_bruta / fator_de_calibracao
const double FATOR_CALIBRACAO_PADRAO = 1.0;

// ---------------------------------------------------------------------------
// PAYLOAD: tudo o que o LoRa transmite. Esta estrutura e convertida em JSON
// antes do envio. Para incluir um novo dado, adicione o campo aqui e a linha
// correspondente em montarJson().
// ---------------------------------------------------------------------------
struct Payload {
  unsigned long tempo_ms;  // tempo desde o inicio do ensaio
  double empuxo;           // empuxo calculado (leitura / fator de calibracao)
  double fatorCalibracao;  // fator de calibracao usado no calculo
};

// ---------------------------------------------------------------------------
// OPCOES: uma opcao para cada acao do sistema. Cada campo aponta para a
// funcao que executa a acao. Quando o ESP recebe um comando, ele chama a
// opcao correspondente (ex.: opcoes.iniciar()).
// ---------------------------------------------------------------------------
struct Opcoes {
  void (*iniciar)();       // inicia o ensaio / leitura
  void (*finalizar)();     // finaliza o ensaio
  void (*ligarLoRa)();     // liga o envio LoRa
  void (*desligarLoRa)();  // desliga o envio LoRa
};

// Acoes (declaradas antes para poderem ser usadas na estrutura Opcoes).
void acaoIniciar();
void acaoFinalizar();
void acaoLigarLoRa();
void acaoDesligarLoRa();

const Opcoes opcoes = {acaoIniciar, acaoFinalizar, acaoLigarLoRa, acaoDesligarLoRa};

HX711 celula;
bool loraPronto = false;
bool ensaioAtivo = false;
bool transmitirLoRa = false;
unsigned long inicioEnsaio = 0;
unsigned long ultimoEnvio = 0;

// Ultimo payload valido (o envio LoRa usa sempre este, assim o intervalo
// de 250 ms nao depende da taxa de amostragem do HX711).
Payload payload = {0, 0.0, FATOR_CALIBRACAO_PADRAO};
bool temLeitura = false;

// Converte o payload em JSON e guarda o texto em buffer.
// Retorna o numero de bytes escritos (0 em caso de falha).
size_t montarJson(const Payload &p, char *buffer, size_t tamanho) {
  JsonDocument doc;
  doc["tempo_ms"] = p.tempo_ms;
  doc["empuxo"] = p.empuxo;
  doc["fator_calibracao"] = p.fatorCalibracao;
  return serializeJson(doc, buffer, tamanho);
}

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
      payload.tempo_ms = millis() - inicioEnsaio;
      payload.empuxo = (payload.fatorCalibracao != 0.0)
                         ? (double)leitura / payload.fatorCalibracao
                         : 0.0;
      temLeitura = true;

      // Saida serial mantida como antes: tempo;leitura_bruta
      Serial.print(payload.tempo_ms);
      Serial.print(';');
      Serial.println(leitura);
    }
  }

  // Envio LoRa: a cada 250 ms enquanto o envio estiver ligado.
  if (ensaioAtivo && loraPronto && transmitirLoRa && temLeitura) {
    unsigned long agora = millis();

    if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
      char json[128];
      size_t tamanho = montarJson(payload, json, sizeof(json));

      if (tamanho > 0 && tamanho < sizeof(json) && LoRa.beginPacket()) {
        LoRa.print(json);
        LoRa.endPacket(true); // Continua lendo enquanto o radio transmite.

        // Mantem o ritmo exato de 4 envios/s (sem acumular atraso).
        ultimoEnvio += INTERVALO_ENVIO_MS;
        // Se ficou muito para tras, ressincroniza.
        if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) ultimoEnvio = agora;
      }
    }
  }
}

// Recebe o comando pela serial e chama a opcao correspondente.
void ReceberComando() {
  if (!Serial.available()) return;
  char comando = Serial.read();

  if (comando == 'i') {
    opcoes.iniciar();
  } else if (comando == 'f') {
    opcoes.finalizar();
  } else if (comando == 'l') {
    // A mesma tecla liga e desliga o envio LoRa.
    if (transmitirLoRa) {
      opcoes.desligarLoRa();
    } else {
      opcoes.ligarLoRa();
    }
  }
}

// ----------------------------- ACOES ---------------------------------------

void acaoIniciar() {
  if (ensaioAtivo) return;
  inicioEnsaio = millis();
  ultimoEnvio = inicioEnsaio;
  transmitirLoRa = false;
  temLeitura = false;
  ensaioAtivo = true;
  Serial.println("tempo_ms;leitura_bruta");
}

void acaoFinalizar() {
  if (!ensaioAtivo) return;
  ensaioAtivo = false;
  transmitirLoRa = false;
  Serial.println("Ensaio encerrado.");
}

void acaoLigarLoRa() {
  if (!ensaioAtivo) {
    Serial.println("Inicie o ensaio com i.");
  } else if (!loraPronto) {
    Serial.println("LoRa indisponivel.");
  } else {
    transmitirLoRa = true;
    // Primeiro envio imediato; depois segue a cada 250 ms.
    ultimoEnvio = millis() - INTERVALO_ENVIO_MS;
    Serial.println("Envio LoRa ligado.");
  }
}

void acaoDesligarLoRa() {
  if (!transmitirLoRa) return;
  transmitirLoRa = false;
  Serial.println("Envio LoRa desligado.");
}
