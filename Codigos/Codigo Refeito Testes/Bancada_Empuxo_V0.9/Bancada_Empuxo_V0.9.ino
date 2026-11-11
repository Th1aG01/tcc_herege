#include <HX711.h>
#include <SPI.h>
#include <LoRa.h>
#include <LittleFS.h>    // memoria flash interna do ESP32 (ja vem na placa)
#include <ArduinoJson.h> // ArduinoJson 7.x (Gerenciador de Bibliotecas)

const int PINO_DT = 33;
const int PINO_SCK = 32;
const int LORA_CS = 21;
const int LORA_RST = 14;
const int LORA_DIO0 = 26;
const int LED_EXEC = 4; // Verde
const int LED_LORA = 2; // Amarelo

// Intervalo entre envios LoRa: 250 ms = 4 envios por segundo.
// Vale tanto para o envio ao vivo quanto para o envio do ensaio gravado.
const unsigned long INTERVALO_ENVIO_MS = 250;

// >>> AJUSTE AQUI: fator de calibracao da celula de carga <<<
// empuxo = leitura_bruta / fator_de_calibracao
const double FATOR_CALIBRACAO_PADRAO = 1.0;

// Arquivo (na flash) onde cada ensaio e gravado. Um novo ensaio apaga o anterior.
const char *ARQUIVO_ENSAIO = "/ensaio.bin";

// ---------------------------------------------------------------------------
// PAYLOAD: tudo o que o LoRa transmite. Esta estrutura e convertida em JSON
// antes do envio. Tambem e o registro gravado na flash a cada amostra.
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
  void (*iniciar)();        // inicia o ensaio SEM LoRa (so grava)
  void (*iniciarComLoRa)(); // inicia o ensaio JA com o LoRa transmitindo ao vivo
  void (*finalizar)();      // finaliza o ensaio
  void (*ligarLoRa)();      // liga o envio LoRa ao vivo (durante o ensaio)
  void (*desligarLoRa)();   // desliga o envio LoRa ao vivo
  void (*enviarGravado)();  // envia pelo LoRa o ensaio gravado (apos finalizar)
  void (*cancelarEnvio)();  // cancela o envio do ensaio gravado
};

// Acoes (declaradas antes para poderem ser usadas na estrutura Opcoes).
void acaoIniciar();
void acaoIniciarComLoRa();
void acaoFinalizar();
void acaoLigarLoRa();
void acaoDesligarLoRa();
void acaoEnviarGravado();
void acaoCancelarEnvio();

const Opcoes opcoes = {acaoIniciar,      acaoIniciarComLoRa, acaoFinalizar,
                       acaoLigarLoRa,    acaoDesligarLoRa,   acaoEnviarGravado,
                       acaoCancelarEnvio};

HX711 celula;
bool loraPronto = false;
bool memoriaPronta = false;
bool ensaioAtivo = false;
bool transmitirLoRa = false;     // envio ao vivo ligado
bool enviandoGravado = false;    // enviando o ensaio gravado
unsigned long inicioEnsaio = 0;
unsigned long ultimoEnvio = 0;

File arquivoEnsaio;
uint32_t amostrasGravadas = 0;
uint32_t amostrasParaEnviar = 0;
uint32_t amostrasEnviadas = 0;

// Ultimo payload valido (o envio ao vivo usa sempre este, assim o intervalo
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

// Monta o JSON e transmite pelo LoRa. Retorna true se o pacote foi enviado.
bool transmitirPayload(const Payload &p) {
  char json[128];
  size_t tamanho = montarJson(p, json, sizeof(json));
  if (tamanho == 0 || tamanho >= sizeof(json)) return false;
  if (!LoRa.beginPacket()) return false; // radio ainda ocupado
  LoRa.print(json);
  LoRa.endPacket(true); // Continua lendo enquanto o radio transmite.
  return true;
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_EXEC, OUTPUT);
  pinMode(LED_LORA, OUTPUT);
  digitalWrite(LED_EXEC, LOW);
  digitalWrite(LED_LORA, LOW);

  memoriaPronta = LittleFS.begin(true); // true = formata na primeira vez
  Serial.println(memoriaPronta ? "Memoria flash pronta." : "Falha na memoria flash.");

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
  Serial.println("Comandos: i=iniciar (sem LoRa); a=iniciar com LoRa ao vivo; f=finalizar;");
  Serial.println("          l=liga/desliga LoRa ao vivo; e=enviar ensaio gravado; c=cancelar envio.");
}

void loop() {
  ReceberComando();
  digitalWrite(LED_EXEC, ensaioAtivo);
  digitalWrite(LED_LORA, (ensaioAtivo && transmitirLoRa) || enviandoGravado);

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

      // Todo ensaio e gravado na flash (assim da para enviar depois pelo LoRa).
      if (arquivoEnsaio.write((const uint8_t *)&payload, sizeof(Payload)) == sizeof(Payload)) {
        amostrasGravadas++;
      } else {
        Serial.println("Memoria cheia.");
        acaoFinalizar();
      }
    }
  }

  // Envio LoRa AO VIVO: a cada 250 ms enquanto estiver ligado.
  if (ensaioAtivo && loraPronto && transmitirLoRa && temLeitura) {
    unsigned long agora = millis();

    if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
      if (transmitirPayload(payload)) {
        // Mantem o ritmo exato de 4 envios/s (sem acumular atraso).
        ultimoEnvio += INTERVALO_ENVIO_MS;
        // Se ficou muito para tras, ressincroniza.
        if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) ultimoEnvio = agora;
      }
    }
  }

  // Envio do ENSAIO GRAVADO: uma amostra a cada 250 ms ate acabar o arquivo.
  if (enviandoGravado) {
    unsigned long agora = millis();

    if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
      Payload gravado;
      if (arquivoEnsaio.read((uint8_t *)&gravado, sizeof(Payload)) == sizeof(Payload)) {
        if (transmitirPayload(gravado)) {
          amostrasEnviadas++;
          ultimoEnvio += INTERVALO_ENVIO_MS;
          if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) ultimoEnvio = agora;
        } else {
          // Radio ocupado: volta uma amostra no arquivo e tenta de novo.
          arquivoEnsaio.seek(arquivoEnsaio.position() - sizeof(Payload));
        }
      } else {
        // Fim do arquivo.
        arquivoEnsaio.close();
        enviandoGravado = false;
        Serial.print("Envio concluido: ");
        Serial.print(amostrasEnviadas);
        Serial.println(" amostras.");
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
  } else if (comando == 'a') {
    opcoes.iniciarComLoRa();
  } else if (comando == 'f') {
    opcoes.finalizar();
  } else if (comando == 'l') {
    // A mesma tecla liga e desliga o envio LoRa ao vivo.
    if (transmitirLoRa) {
      opcoes.desligarLoRa();
    } else {
      opcoes.ligarLoRa();
    }
  } else if (comando == 'e') {
    opcoes.enviarGravado();
  } else if (comando == 'c') {
    opcoes.cancelarEnvio();
  }
}

// ----------------------------- ACOES ---------------------------------------

void acaoIniciar() {
  if (ensaioAtivo) return;
  if (enviandoGravado) {
    Serial.println("Aguarde o envio terminar (c cancela).");
    return;
  }
  if (!memoriaPronta) {
    Serial.println("Memoria flash indisponivel.");
    return;
  }
  arquivoEnsaio = LittleFS.open(ARQUIVO_ENSAIO, FILE_WRITE); // apaga o ensaio anterior
  if (!arquivoEnsaio) {
    Serial.println("Nao foi possivel criar o arquivo do ensaio.");
    return;
  }
  amostrasGravadas = 0;
  inicioEnsaio = millis();
  ultimoEnvio = inicioEnsaio;
  transmitirLoRa = false;
  temLeitura = false;
  ensaioAtivo = true;
  Serial.println("tempo_ms;leitura_bruta");
}

void acaoIniciarComLoRa() {
  acaoIniciar();
  if (ensaioAtivo) acaoLigarLoRa();
}

void acaoFinalizar() {
  if (!ensaioAtivo) return;
  ensaioAtivo = false;
  transmitirLoRa = false;
  arquivoEnsaio.close();
  Serial.print("Ensaio encerrado. Amostras gravadas: ");
  Serial.println(amostrasGravadas);
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

void acaoEnviarGravado() {
  if (ensaioAtivo) {
    Serial.println("Finalize o ensaio com f antes de enviar.");
    return;
  }
  if (enviandoGravado) return;
  if (!loraPronto) {
    Serial.println("LoRa indisponivel.");
    return;
  }
  if (!memoriaPronta) {
    Serial.println("Memoria flash indisponivel.");
    return;
  }
  arquivoEnsaio = LittleFS.open(ARQUIVO_ENSAIO, FILE_READ);
  if (!arquivoEnsaio || arquivoEnsaio.size() < sizeof(Payload)) {
    if (arquivoEnsaio) arquivoEnsaio.close();
    Serial.println("Nenhum ensaio gravado.");
    return;
  }
  amostrasParaEnviar = arquivoEnsaio.size() / sizeof(Payload);
  amostrasEnviadas = 0;
  ultimoEnvio = millis() - INTERVALO_ENVIO_MS; // primeiro envio imediato
  enviandoGravado = true;
  Serial.print("Enviando ensaio gravado: ");
  Serial.print(amostrasParaEnviar);
  Serial.println(" amostras.");
}

void acaoCancelarEnvio() {
  if (!enviandoGravado) return;
  arquivoEnsaio.close();
  enviandoGravado = false;
  Serial.print("Envio cancelado apos ");
  Serial.print(amostrasEnviadas);
  Serial.println(" amostras.");
}
