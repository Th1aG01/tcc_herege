// ===========================================================================
// Bancada de Empuxo - firmware do ESP32 - VERSAO 0.10
//
// Novidades da 0.10 (a 0.9 foi mantida sem alteracoes em outra pasta):
//  - Comandos por linha de texto (ex.: "COEF 1234.5"), com resposta em JSON.
//  - Empuxo sempre em kgf: empuxo = leitura_corrigida / coeficiente.
//    O coeficiente vem do computador (comando COEF); o ESP nao o guarda.
//  - Tara (TARA) e uma operacao interna do ESP. Nao existe offset por comando.
//  - Iniciar ensaio e iniciar motor sao comandos separados. Nesta versao o
//    motor ainda NAO e acionado (MOTOR_INICIAR responde com erro).
//  - Erros e avisos estruturados em JSON, prontos para o log da interface.
//  - O HX711 ausente nao trava mais o ESP (V0_9 travava no tare do setup).
//
// Protocolo serial: 115200 baud. Cada comando termina com Enter (\n).
// Toda linha de resposta que comeca com '{' e um JSON. Linhas que comecam
// com '#' sao texto para pessoas e devem ser ignoradas por programas.
// Digite AJUDA para ver os comandos.
// ===========================================================================
#include <HX711.h>
#include <SPI.h>
#include <LoRa.h>
#include <LittleFS.h>    // memoria flash interna do ESP32 (ja vem na placa)
#include <ArduinoJson.h> // ArduinoJson 7.x (Gerenciador de Bibliotecas)
#include <cmath>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

const char *VERSAO_FIRMWARE = "0.10";

const int PINO_DT = 33;
const int PINO_SCK = 32;
const int LORA_CS = 21;
const int LORA_RST = 14;
const int LORA_DIO0 = 26;
const int LED_EXEC = 4; // Verde: aceso durante o ensaio (aquisicao)
const int LED_LORA = 2; // Amarelo: aceso durante a transmissao LoRa

// Intervalo entre envios LoRa: 250 ms = 4 envios por segundo.
// Vale tanto para o envio ao vivo quanto para o envio do ensaio gravado.
const unsigned long INTERVALO_ENVIO_MS = 250;

// Se o HX711 ficar tanto tempo sem entregar uma leitura, ele e dado como
// sem resposta (durante o ensaio isso gera um erro estruturado).
const unsigned long TIMEOUT_HX711_MS = 1000;

// Arquivo (na flash) onde cada ensaio e gravado. Um novo ensaio apaga o anterior.
const char *ARQUIVO_ENSAIO = "/ensaio.bin";

const size_t TAMANHO_LINHA = 64; // tamanho maximo de um comando

// ---------------------------------------------------------------------------
// PAYLOAD: tudo o que o LoRa transmite. Esta estrutura e convertida em JSON
// antes do envio. Tambem e o registro gravado na flash a cada amostra.
// (Mesmos campos da V0_9; agora empuxo esta em kgf e fatorCalibracao e o
// coeficiente recebido do computador.)
// ---------------------------------------------------------------------------
struct Payload {
  unsigned long tempo_ms;  // tempo desde o inicio do ensaio
  double empuxo;           // empuxo em kgf (leitura corrigida / coeficiente)
  double fatorCalibracao;  // coeficiente de calibracao usado no calculo
};

// ---------------------------------------------------------------------------
// OPCOES: uma opcao para cada acao do sistema. Cada campo aponta para a
// funcao que executa a acao. Quando o ESP recebe um comando, ele chama a
// opcao correspondente (ex.: opcoes.iniciar()). Cada acao responde sozinha
// (JSON "ok" ou "erro").
// ---------------------------------------------------------------------------
struct Opcoes {
  void (*iniciar)();                          // inicia o ensaio SEM LoRa (so grava)
  void (*iniciarComLoRa)();                   // inicia o ensaio JA com LoRa ao vivo
  void (*finalizar)();                        // finaliza o ensaio
  void (*ligarLoRa)();                        // liga o envio LoRa ao vivo
  void (*desligarLoRa)();                     // desliga o envio LoRa ao vivo
  void (*enviarGravado)();                    // envia pelo LoRa o ensaio gravado
  void (*cancelarEnvio)();                    // cancela o envio do ensaio gravado
  void (*definirCoeficiente)(const char *);   // recebe o coeficiente (kgf) do computador
  void (*tara)();                             // zera a leitura (tara interna do ESP)
  void (*lerAgora)();                         // uma leitura avulsa, fora do ensaio
  void (*enviarStatus)();                     // estado atual do firmware
  void (*iniciarMotor)();                     // reservado: ainda nao aciona nada
};

// Prototipos (o Arduino IDE os cria sozinho, mas assim ficam explicitos).
void ReceberComando();
void executarLinha(char *linha);
void acaoIniciar();
void acaoIniciarComLoRa();
void acaoFinalizar();
void acaoLigarLoRa();
void acaoDesligarLoRa();
void acaoEnviarGravado();
void acaoCancelarEnvio();
void acaoDefinirCoeficiente(const char *texto);
void acaoTara();
void acaoLerAgora();
void acaoEnviarStatus();
void acaoIniciarMotor();

const Opcoes opcoes = {acaoIniciar,        acaoIniciarComLoRa,     acaoFinalizar,
                       acaoLigarLoRa,      acaoDesligarLoRa,       acaoEnviarGravado,
                       acaoCancelarEnvio,  acaoDefinirCoeficiente, acaoTara,
                       acaoLerAgora,       acaoEnviarStatus,       acaoIniciarMotor};

HX711 celula;
bool loraPronto = false;
bool memoriaPronta = false;
bool ensaioAtivo = false;
bool transmitirLoRa = false;     // envio ao vivo ligado
bool enviandoGravado = false;    // enviando o ensaio gravado
unsigned long inicioEnsaio = 0;
unsigned long ultimoEnvio = 0;

// Calibracao: o computador envia o coeficiente com o comando COEF.
// Sem ele o ESP nao inicia ensaio (nao ha como converter para kgf).
bool calibrado = false;
double coeficienteKgf = 0.0;

// Controle de resposta do HX711.
bool hx711Visto = false;          // ja entregou alguma leitura
unsigned long ultimaLeituraMs = 0;
bool hx711Falhou = false;         // erro "sem dados" ja informado neste ensaio

File arquivoEnsaio;
uint32_t amostrasGravadas = 0;
uint32_t amostrasParaEnviar = 0;
uint32_t amostrasEnviadas = 0;

// Ultimo payload valido (o envio ao vivo usa sempre este, assim o intervalo
// de 250 ms nao depende da taxa de amostragem do HX711).
Payload payload = {0, 0.0, 0.0};
bool temLeitura = false;

// ---------------------------------------------------------------------------
// SAIDA JSON: monta uma linha em um buffer (com escape de texto) e envia.
// Exemplo: LinhaJson("ok").texto("comando", "TARA").enviar();
//          -> {"tipo":"ok","comando":"TARA"}
// Numeros nao finitos saem como null. Se a linha nao couber no buffer, sai um
// erro fixo no lugar (nunca uma linha JSON quebrada).
// ---------------------------------------------------------------------------
class LinhaJson {
 public:
  explicit LinhaJson(const char *tipo) {
    buf[0] = 0;
    caractere('{');
    bruto("\"tipo\":");
    textoEscapado(tipo);
  }
  LinhaJson &texto(const char *chave, const char *valor) {
    abrirChave(chave);
    textoEscapado(valor);
    return *this;
  }
  LinhaJson &inteiro(const char *chave, long valor) {
    char t[24];
    snprintf(t, sizeof(t), "%ld", valor);
    abrirChave(chave);
    bruto(t);
    return *this;
  }
  LinhaJson &natural(const char *chave, unsigned long valor) {
    char t[24];
    snprintf(t, sizeof(t), "%lu", valor);
    abrirChave(chave);
    bruto(t);
    return *this;
  }
  LinhaJson &numero(const char *chave, double valor, int digitos = 8) {
    abrirChave(chave);
    if (std::isfinite(valor)) {
      char t[40];
      snprintf(t, sizeof(t), "%.*g", digitos, valor);
      bruto(t);
    } else {
      bruto("null");
    }
    return *this;
  }
  LinhaJson &logico(const char *chave, bool valor) {
    abrirChave(chave);
    bruto(valor ? "true" : "false");
    return *this;
  }
  LinhaJson &nulo(const char *chave) {
    abrirChave(chave);
    bruto("null");
    return *this;
  }
  void enviar() {
    caractere('}');
    if (estouro) {
      Serial.println("{\"tipo\":\"erro\",\"nivel\":\"ERRO\",\"codigo\":\"JSON_MUITO_LONGO\","
                     "\"origem\":\"firmware\",\"mensagem\":\"Resposta maior que o buffer.\","
                     "\"detalhes\":\"\"}");
    } else {
      Serial.println(buf);
    }
  }

 private:
  char buf[512];
  size_t n = 0;
  bool estouro = false;

  void caractere(char c) {
    if (n + 1 >= sizeof(buf)) { estouro = true; return; }
    buf[n++] = c;
    buf[n] = 0;
  }
  void bruto(const char *s) {
    while (*s) caractere(*s++);
  }
  void abrirChave(const char *chave) {
    caractere(',');
    caractere('"');
    bruto(chave);
    bruto("\":");
  }
  void textoEscapado(const char *s) {
    caractere('"');
    for (; *s; s++) {
      unsigned char c = (unsigned char)*s;
      if (c == '"' || c == '\\') { caractere('\\'); caractere((char)c); }
      else if (c < 0x20 || c > 0x7E) caractere('?'); // controle/nao ASCII
      else caractere((char)c);
    }
    caractere('"');
  }
};

// ---------------------------------------------------------------------------
// ERROS ESTRUTURADOS. Campos: nivel (ERRO ou AVISO), codigo (estavel, para o
// programa decidir o que fazer), origem, mensagem, detalhes e t_ms (tempo
// desde que o ESP ligou). Correspondem a registrar_erro(origem, mensagem,
// detalhes, nivel) da interface. Mensagens sem acento de proposito.
// ---------------------------------------------------------------------------
void registrarErro(const char *codigo, const char *origem, const char *mensagem,
                   const char *detalhes = "", const char *nivel = "ERRO") {
  LinhaJson("erro")
      .texto("nivel", nivel)
      .texto("codigo", codigo)
      .texto("origem", origem)
      .texto("mensagem", mensagem)
      .texto("detalhes", detalhes)
      .natural("t_ms", millis())
      .enviar();
}

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

// ----------------------------- HX711 ---------------------------------------

// Espera o HX711 ter um dado novo, com limite de tempo. Evita travar o ESP
// quando o modulo esta desligado ou com fio solto (is_ready() ja era usado
// na V0_9; tare() e get_value() esperam para sempre sem isto).
bool aguardarHx711(unsigned long timeoutMs) {
  unsigned long inicio = millis();
  while (!celula.is_ready()) {
    if (millis() - inicio >= timeoutMs) return false;
    delay(1);
  }
  return true;
}

bool hx711Respondendo() {
  return hx711Visto && (millis() - ultimaLeituraMs) < TIMEOUT_HX711_MS;
}

void erroHx711SemDados(const char *quando) {
  char detalhes[96];
  snprintf(detalhes, sizeof(detalhes), "%s. Pinos: DT=GPIO%d, SCK=GPIO%d.", quando, PINO_DT, PINO_SCK);
  registrarErro("HX711_SEM_DADOS", "hx711",
                "O HX711 nao respondeu. Confira alimentacao e ligacao da celula.", detalhes);
}

// Tara interna: usada no setup e pelo comando TARA. Retorna false (e ja
// informa o erro) se o HX711 nao responder.
bool zerarCelula() {
  if (!aguardarHx711(TIMEOUT_HX711_MS)) {
    erroHx711SemDados("Sem leitura durante a tara");
    return false;
  }
  celula.tare();
  hx711Visto = true;
  ultimaLeituraMs = millis();
  return true;
}

// ----------------------------- SETUP / LOOP --------------------------------

void setup() {
  Serial.begin(115200);

  pinMode(LED_EXEC, OUTPUT);
  pinMode(LED_LORA, OUTPUT);
  digitalWrite(LED_EXEC, LOW);
  digitalWrite(LED_LORA, LOW);

  memoriaPronta = LittleFS.begin(true); // true = formata na primeira vez
  if (!memoriaPronta) {
    registrarErro("MEMORIA_INDISPONIVEL", "memoria", "Falha ao iniciar a memoria flash (LittleFS).");
  }

  SPI.begin(18, 19, 23, LORA_CS); // SCK, MISO, MOSI, CS
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  LoRa.setSPIFrequency(1000000);
  loraPronto = LoRa.begin(915E6);

  if (loraPronto) {
    LoRa.setSyncWord(145);
    LoRa.setTxPower(2);
  } else {
    registrarErro("LORA_INDISPONIVEL", "lora", "Falha ao iniciar o LoRa. Confira o modulo e a ligacao SPI.");
  }

  celula.begin(PINO_DT, PINO_SCK);
  zerarCelula(); // tara inicial, como na V0_9 (agora sem travar se o HX711 faltar)

  LinhaJson("info")
      .texto("evento", "inicializado")
      .texto("firmware", VERSAO_FIRMWARE)
      .texto("unidade", "kgf")
      .logico("memoria_pronta", memoriaPronta)
      .logico("lora_pronto", loraPronto)
      .logico("hx711_ok", hx711Respondendo())
      .logico("calibrado", calibrado)
      .enviar();
  Serial.println("# Digite AJUDA para ver os comandos. Termine cada comando com Enter (Novo avanco de linha).");
}

void loop() {
  ReceberComando();
  digitalWrite(LED_EXEC, ensaioAtivo);
  digitalWrite(LED_LORA, (ensaioAtivo && transmitirLoRa) || enviandoGravado);

  // Leitura da celula (so quando o HX711 tem um dado novo).
  if (celula.is_ready()) {
    long leitura = celula.get_value(); // ja descontada a tara
    hx711Visto = true;
    ultimaLeituraMs = millis();

    if (ensaioAtivo) {
      if (hx711Falhou) {
        hx711Falhou = false;
        LinhaJson("evento").texto("evento", "hx711_recuperado").enviar();
      }
      payload.tempo_ms = millis() - inicioEnsaio;
      payload.empuxo = (payload.fatorCalibracao != 0.0)
                         ? (double)leitura / payload.fatorCalibracao
                         : 0.0;
      temLeitura = true;

      // Uma linha por amostra: tempo, leitura corrigida (contagens) e kgf.
      LinhaJson("amostra")
          .natural("t_ms", payload.tempo_ms)
          .inteiro("leitura", leitura)
          .numero("kgf", payload.empuxo)
          .enviar();

      // Todo ensaio e gravado na flash (assim da para enviar depois pelo LoRa).
      if (arquivoEnsaio.write((const uint8_t *)&payload, sizeof(Payload)) == sizeof(Payload)) {
        amostrasGravadas++;
      } else {
        registrarErro("MEMORIA_CHEIA", "memoria",
                      "Memoria flash cheia. O ensaio foi finalizado.");
        unsigned long duracao = millis() - inicioEnsaio;
        arquivoEnsaio.close();
        ensaioAtivo = false;
        transmitirLoRa = false;
        LinhaJson("evento")
            .texto("evento", "ensaio_finalizado")
            .texto("motivo", "memoria_cheia")
            .natural("amostras", amostrasGravadas)
            .natural("duracao_ms", duracao)
            .enviar();
      }
    }
  }

  // HX711 parou de responder no meio do ensaio: informa uma vez.
  if (ensaioAtivo && !hx711Falhou && (millis() - ultimaLeituraMs) > TIMEOUT_HX711_MS) {
    hx711Falhou = true;
    erroHx711SemDados("Sem leituras durante o ensaio");
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
        LinhaJson("evento")
            .texto("evento", "envio_concluido")
            .natural("amostras", amostrasEnviadas)
            .enviar();
      }
    }
  }
}

// ----------------------------- COMANDOS ------------------------------------

// Junta caracteres ate o Enter e entrega a linha completa para executarLinha.
void ReceberComando() {
  static char linha[TAMANHO_LINHA];
  static size_t tamanho = 0;
  static bool excedeu = false;

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (excedeu) {
        registrarErro("LINHA_LONGA", "serial", "Comando maior que o limite. Foi ignorado.",
                      "Limite: 63 caracteres.");
      } else {
        linha[tamanho] = 0;
        executarLinha(linha);
      }
      tamanho = 0;
      excedeu = false;
      continue;
    }
    if (excedeu) continue;
    if (tamanho < TAMANHO_LINHA - 1) linha[tamanho++] = c;
    else excedeu = true;
  }
}

bool igual(const char *a, const char *b) { return strcasecmp(a, b) == 0; }

// Separa "COMANDO argumento" e chama a opcao correspondente.
// Aceita maiusculas ou minusculas. As letras da V0_9 (i a f l e c) continuam
// funcionando, agora seguidas de Enter.
void executarLinha(char *linha) {
  while (isspace((unsigned char)*linha)) linha++;
  size_t n = strlen(linha);
  while (n > 0 && isspace((unsigned char)linha[n - 1])) linha[--n] = 0;
  if (n == 0) return; // linha vazia

  char *argumento = linha;
  while (*argumento && !isspace((unsigned char)*argumento)) argumento++;
  if (*argumento) {
    *argumento++ = 0;
    while (isspace((unsigned char)*argumento)) argumento++;
  }
  const char *comando = linha;
  bool temArgumento = argumento[0] != 0;
  char detalhes[96];

  if (igual(comando, "COEF")) {
    opcoes.definirCoeficiente(argumento);
    return;
  }
  if (igual(comando, "INICIAR") || igual(comando, "I")) {
    if (!temArgumento) {
      opcoes.iniciar();
    } else if (igual(argumento, "LORA")) {
      opcoes.iniciarComLoRa();
    } else {
      snprintf(detalhes, sizeof(detalhes), "Recebido: %.40s. Use INICIAR ou INICIAR LORA.", argumento);
      registrarErro("ARGUMENTO_INVALIDO", "comando", "Argumento invalido em INICIAR.", detalhes);
    }
    return;
  }

  // Os demais comandos nao usam argumento.
  if (temArgumento) {
    snprintf(detalhes, sizeof(detalhes), "Recebido: %.40s %.40s", comando, argumento);
    registrarErro("ARGUMENTO_INVALIDO", "comando", "Este comando nao usa argumento.", detalhes);
    return;
  }
  if (igual(comando, "A")) opcoes.iniciarComLoRa();
  else if (igual(comando, "FINALIZAR") || igual(comando, "F")) opcoes.finalizar();
  else if (igual(comando, "LORA_LIGAR")) opcoes.ligarLoRa();
  else if (igual(comando, "LORA_DESLIGAR")) opcoes.desligarLoRa();
  else if (igual(comando, "L")) {
    // A mesma tecla liga e desliga o envio LoRa ao vivo (como na V0_9).
    if (transmitirLoRa) opcoes.desligarLoRa();
    else opcoes.ligarLoRa();
  }
  else if (igual(comando, "ENVIAR") || igual(comando, "E")) opcoes.enviarGravado();
  else if (igual(comando, "CANCELAR") || igual(comando, "C")) opcoes.cancelarEnvio();
  else if (igual(comando, "TARA")) opcoes.tara();
  else if (igual(comando, "LER")) opcoes.lerAgora();
  else if (igual(comando, "STATUS")) opcoes.enviarStatus();
  else if (igual(comando, "MOTOR_INICIAR")) opcoes.iniciarMotor();
  else if (igual(comando, "AJUDA") || igual(comando, "?")) {
    Serial.println("# COEF <kgf>       define o coeficiente (leitura corrigida / coeficiente = kgf)");
    Serial.println("# TARA             zera a leitura (so fora do ensaio)");
    Serial.println("# LER              uma leitura avulsa em contagens e kgf (so fora do ensaio)");
    Serial.println("# INICIAR          inicia o ensaio sem LoRa (grava na flash)");
    Serial.println("# INICIAR LORA     inicia o ensaio com LoRa ao vivo");
    Serial.println("# FINALIZAR        finaliza o ensaio");
    Serial.println("# LORA_LIGAR       liga o LoRa ao vivo (durante o ensaio)");
    Serial.println("# LORA_DESLIGAR    desliga o LoRa ao vivo");
    Serial.println("# ENVIAR           envia o ensaio gravado pelo LoRa (apos finalizar)");
    Serial.println("# CANCELAR         cancela o envio do ensaio gravado");
    Serial.println("# STATUS           estado do firmware");
    Serial.println("# MOTOR_INICIAR    reservado: nesta versao nao aciona o motor");
    Serial.println("# Atalhos da V0_9: i a f l e c");
  } else {
    snprintf(detalhes, sizeof(detalhes), "Recebido: %.40s. Digite AJUDA.", comando);
    registrarErro("COMANDO_DESCONHECIDO", "comando", "Comando desconhecido.", detalhes);
  }
}

// Le um numero decimal (ponto ou virgula, sem separador de milhar).
// Recusa vazio, texto sobrando, hexadecimal, nan e inf.
bool lerNumero(const char *texto, double &valor) {
  char copia[TAMANHO_LINHA];
  size_t n = strlen(texto);
  if (n == 0 || n >= sizeof(copia)) return false;
  bool temVirgula = false, temPonto = false;
  for (size_t i = 0; i < n; i++) {
    char c = texto[i];
    if (c == ',') { temVirgula = true; c = '.'; }
    else if (c == '.') temPonto = true;
    else if (!(isdigit((unsigned char)c) || c == '+' || c == '-' || c == 'e' || c == 'E')) return false;
    copia[i] = c;
  }
  copia[n] = 0;
  if (temVirgula && temPonto) return false;
  char *fim = nullptr;
  errno = 0;
  double v = strtod(copia, &fim);
  if (fim == copia || *fim != 0 || errno == ERANGE || !std::isfinite(v)) return false;
  valor = v;
  return true;
}

// ----------------------------- ACOES ---------------------------------------

void erroEnsaioEmAndamento(const char *mensagem) {
  registrarErro("ENSAIO_EM_ANDAMENTO", "ensaio", mensagem);
}

void erroEnvioEmAndamento() {
  registrarErro("ENVIO_EM_ANDAMENTO", "lora", "Aguarde o envio terminar (CANCELAR interrompe).");
}

void acaoDefinirCoeficiente(const char *texto) {
  if (ensaioAtivo) {
    erroEnsaioEmAndamento("Nao e possivel trocar o coeficiente durante o ensaio.");
    return;
  }
  char detalhes[96];
  snprintf(detalhes, sizeof(detalhes), "Recebido: \"%.60s\"", texto);
  double valor;
  if (!lerNumero(texto, valor)) {
    registrarErro("COEF_INVALIDO", "calibracao",
                  "Coeficiente invalido. Use COEF <numero> (kgf), com ponto decimal.", detalhes);
    return;
  }
  if (valor == 0.0) {
    registrarErro("COEF_INVALIDO", "calibracao", "O coeficiente nao pode ser zero.", detalhes);
    return;
  }
  coeficienteKgf = valor;
  calibrado = true;
  LinhaJson("ok")
      .texto("comando", "COEF")
      .numero("coeficiente_kgf", coeficienteKgf, 10)
      .texto("unidade", "kgf")
      .enviar();
}

void acaoTara() {
  if (ensaioAtivo) {
    erroEnsaioEmAndamento("A tara so pode ser feita fora do ensaio.");
    return;
  }
  if (enviandoGravado) {
    erroEnvioEmAndamento();
    return;
  }
  if (!zerarCelula()) return; // o erro ja foi informado
  LinhaJson("ok").texto("comando", "TARA").enviar();
}

void acaoLerAgora() {
  if (ensaioAtivo) {
    erroEnsaioEmAndamento("Durante o ensaio use as linhas de amostra.");
    return;
  }
  if (!aguardarHx711(TIMEOUT_HX711_MS)) {
    erroHx711SemDados("Sem leitura em LER");
    return;
  }
  long leitura = celula.get_value();
  hx711Visto = true;
  ultimaLeituraMs = millis();
  LinhaJson resposta("ok");
  resposta.texto("comando", "LER").inteiro("leitura", leitura).logico("calibrado", calibrado);
  if (calibrado) resposta.numero("kgf", (double)leitura / coeficienteKgf);
  else resposta.nulo("kgf"); // sem coeficiente nao ha kgf
  resposta.enviar();
}

// Inicia o ensaio. NAO aciona o motor (isso sera outro comando).
void iniciarEnsaio(bool comLoRa) {
  if (ensaioAtivo) {
    erroEnsaioEmAndamento("Ja existe um ensaio em andamento.");
    return;
  }
  if (enviandoGravado) {
    erroEnvioEmAndamento();
    return;
  }
  if (!memoriaPronta) {
    registrarErro("MEMORIA_INDISPONIVEL", "memoria", "Memoria flash indisponivel.");
    return;
  }
  if (!calibrado) {
    registrarErro("SEM_CALIBRACAO", "calibracao", "Envie COEF <kgf> antes de iniciar o ensaio.");
    return;
  }
  if (comLoRa && !loraPronto) {
    registrarErro("LORA_INDISPONIVEL", "lora", "LoRa indisponivel. O ensaio nao foi iniciado.");
    return;
  }
  if (!aguardarHx711(TIMEOUT_HX711_MS)) {
    erroHx711SemDados("Sem leitura ao iniciar o ensaio");
    return;
  }
  arquivoEnsaio = LittleFS.open(ARQUIVO_ENSAIO, FILE_WRITE); // apaga o ensaio anterior
  if (!arquivoEnsaio) {
    registrarErro("ARQUIVO_ENSAIO", "memoria", "Nao foi possivel criar o arquivo do ensaio.");
    return;
  }
  payload.fatorCalibracao = coeficienteKgf;
  amostrasGravadas = 0;
  inicioEnsaio = millis();
  ultimoEnvio = inicioEnsaio;
  ultimaLeituraMs = inicioEnsaio;
  transmitirLoRa = false;
  temLeitura = false;
  hx711Falhou = false;
  ensaioAtivo = true;
  if (comLoRa) {
    transmitirLoRa = true;
    ultimoEnvio = inicioEnsaio - INTERVALO_ENVIO_MS; // primeiro envio imediato
  }
  LinhaJson("ok")
      .texto("comando", "INICIAR")
      .texto("modo", comLoRa ? "lora_ao_vivo" : "sem_lora")
      .numero("coeficiente_kgf", coeficienteKgf, 10)
      .enviar();
}

void acaoIniciar() { iniciarEnsaio(false); }

void acaoIniciarComLoRa() { iniciarEnsaio(true); }

void acaoFinalizar() {
  if (!ensaioAtivo) {
    // Parar duas vezes nao e erro: informa que nada mudou.
    LinhaJson("ok").texto("comando", "FINALIZAR").logico("alterou", false).enviar();
    return;
  }
  unsigned long duracao = millis() - inicioEnsaio;
  ensaioAtivo = false;
  transmitirLoRa = false;
  arquivoEnsaio.close();
  LinhaJson("ok")
      .texto("comando", "FINALIZAR")
      .logico("alterou", true)
      .natural("amostras", amostrasGravadas)
      .natural("duracao_ms", duracao)
      .enviar();
}

void acaoLigarLoRa() {
  if (!ensaioAtivo) {
    registrarErro("ENSAIO_NAO_ATIVO", "ensaio", "Inicie o ensaio antes de ligar o LoRa ao vivo.");
  } else if (!loraPronto) {
    registrarErro("LORA_INDISPONIVEL", "lora", "LoRa indisponivel.");
  } else {
    transmitirLoRa = true;
    // Primeiro envio imediato; depois segue a cada 250 ms.
    ultimoEnvio = millis() - INTERVALO_ENVIO_MS;
    LinhaJson("ok").texto("comando", "LORA_LIGAR").enviar();
  }
}

void acaoDesligarLoRa() {
  bool estava = transmitirLoRa;
  transmitirLoRa = false;
  LinhaJson("ok").texto("comando", "LORA_DESLIGAR").logico("alterou", estava).enviar();
}

void acaoEnviarGravado() {
  if (ensaioAtivo) {
    erroEnsaioEmAndamento("Finalize o ensaio com FINALIZAR antes de enviar.");
    return;
  }
  if (enviandoGravado) {
    erroEnvioEmAndamento();
    return;
  }
  if (!loraPronto) {
    registrarErro("LORA_INDISPONIVEL", "lora", "LoRa indisponivel.");
    return;
  }
  if (!memoriaPronta) {
    registrarErro("MEMORIA_INDISPONIVEL", "memoria", "Memoria flash indisponivel.");
    return;
  }
  arquivoEnsaio = LittleFS.open(ARQUIVO_ENSAIO, FILE_READ);
  if (!arquivoEnsaio || arquivoEnsaio.size() < sizeof(Payload)) {
    if (arquivoEnsaio) arquivoEnsaio.close();
    registrarErro("NENHUM_ENSAIO_GRAVADO", "memoria", "Nao ha ensaio gravado para enviar.");
    return;
  }
  amostrasParaEnviar = arquivoEnsaio.size() / sizeof(Payload);
  amostrasEnviadas = 0;
  ultimoEnvio = millis() - INTERVALO_ENVIO_MS; // primeiro envio imediato
  enviandoGravado = true;
  LinhaJson("ok")
      .texto("comando", "ENVIAR")
      .natural("amostras", amostrasParaEnviar)
      .enviar();
}

void acaoCancelarEnvio() {
  if (!enviandoGravado) {
    LinhaJson("ok").texto("comando", "CANCELAR").logico("alterou", false).enviar();
    return;
  }
  arquivoEnsaio.close();
  enviandoGravado = false;
  LinhaJson("ok")
      .texto("comando", "CANCELAR")
      .logico("alterou", true)
      .natural("amostras_enviadas", amostrasEnviadas)
      .enviar();
}

void acaoEnviarStatus() {
  LinhaJson resposta("ok");
  resposta.texto("comando", "STATUS")
      .texto("firmware", VERSAO_FIRMWARE)
      .texto("unidade", "kgf")
      .logico("ensaio_ativo", ensaioAtivo)
      .logico("lora_ao_vivo", transmitirLoRa)
      .logico("enviando_gravado", enviandoGravado)
      .logico("calibrado", calibrado);
  if (calibrado) resposta.numero("coeficiente_kgf", coeficienteKgf, 10);
  else resposta.nulo("coeficiente_kgf");
  resposta.logico("lora_pronto", loraPronto)
      .logico("memoria_pronta", memoriaPronta)
      .logico("hx711_ok", hx711Respondendo())
      .natural("amostras_gravadas", amostrasGravadas)
      .natural("amostras_para_enviar", amostrasParaEnviar)
      .natural("amostras_enviadas", amostrasEnviadas)
      .natural("uptime_ms", millis())
      .enviar();
}

// Reservado para a etapa do motor. Nao existe pino de ignicao definido
// neste firmware, entao nada e acionado.
void acaoIniciarMotor() {
  registrarErro("MOTOR_NAO_IMPLEMENTADO", "motor",
                "O acionamento do motor ainda nao existe nesta versao. Nada foi acionado.");
}
