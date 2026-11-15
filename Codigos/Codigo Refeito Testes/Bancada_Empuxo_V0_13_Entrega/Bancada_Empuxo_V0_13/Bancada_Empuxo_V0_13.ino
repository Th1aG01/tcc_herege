// Bancada de Empuxo V0.13 - Protocolo USB JSON 2 e diagnosticos.
// Base: V0.12. USB: objetos JSON completos terminados por LF (protocolo 2).
// Todas as mensagens da aplicacao na serial sao JSON, uma por linha.
#include <Arduino.h>
#include <esp_random.h>
#include <HX711.h>
#include <SPI.h>
#include <LoRa.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // ArduinoJson 7.x
#include <ctype.h>
#include <math.h>
#include <string.h>

const char *VERSAO_FIRMWARE = "0.13";
const int VERSAO_PROTOCOLO = 2;

// Pinagem e configuracao de radio preservadas da V0.9 anexada.
const int PINO_DT = 33;
const int PINO_SCK = 32;
const int LORA_CS = 21;
const int LORA_RST = 14;
const int LORA_DIO0 = 26;
const int LED_EXEC = 4; // Verde
const int LED_LORA = 2; // Amarelo
const unsigned long INTERVALO_ENVIO_MS = 250;

// Coeficiente em contagens/kgf; ainda definido no firmware nesta etapa.
// 1.0 e o valor original de demonstracao, NAO uma calibracao fisica validada.
const double FATOR_CALIBRACAO_PADRAO = 1.0;
const char *ARQUIVO_ENSAIO = "/ensaio.bin";
const unsigned long TIMEOUT_HX711_MS = 2000;
const unsigned long TIMEOUT_TARA_MS = 3000;


// Layout binario e nomes do JSON LoRa preservados da V0.9.
struct Payload {
  unsigned long tempo_ms;
  double empuxo;          // kgf, quando o coeficiente estiver calibrado
  double fatorCalibracao;
};

bool acaoIniciar();
bool acaoIniciarComLoRa();
bool acaoFinalizar();
bool acaoLigarLoRa();
bool acaoDesligarLoRa();
bool acaoEnviarGravado();
bool acaoCancelarEnvio();

struct Opcoes {
  bool (*iniciar)();
  bool (*iniciarComLoRa)();
  bool (*finalizar)();
  bool (*ligarLoRa)();
  bool (*desligarLoRa)();
  bool (*enviarGravado)();
  bool (*cancelarEnvio)();
};

const Opcoes opcoes = {acaoIniciar, acaoIniciarComLoRa, acaoFinalizar,
                       acaoLigarLoRa, acaoDesligarLoRa, acaoEnviarGravado,
                       acaoCancelarEnvio};

HX711 celula;
bool loraPronto = false;
volatile bool loraTransmitindo = false; // atualizado pela interrupcao TX_DONE
bool memoriaPronta = false;
bool taraPronta = false;
bool taraInicialAtiva = false;
bool hx711Respondendo = false;
bool ensaioAtivo = false;
bool transmitirLoRa = false;
bool enviandoGravado = false;
unsigned long inicioEnsaio = 0;
unsigned long ultimoEnvio = 0;
unsigned long ultimaLeituraHX = 0;
File arquivoEnsaio;
uint32_t amostrasGravadas = 0;
uint32_t amostrasParaEnviar = 0;
uint32_t amostrasEnviadas = 0;
Payload payload = {0, 0.0, FATOR_CALIBRACAO_PADRAO};
bool temLeitura = false;

// -------------------------- USB / PROTOCOLO 2 -----------------------------
const size_t RX_MAX = 512;
const size_t TX_MAX = 8192;
const size_t RESPOSTA_MAX = 1536;
const unsigned long RX_TIMEOUT_MS = 1000;
const uint8_t HISTORICO_MAX = 8;
struct RegistroUSB {
  uint32_t id = 0;
  char comando[25] = {};
  bool pendente = false;
  char resposta[RESPOSTA_MAX] = {};
  char conclusao[768] = {};
};
RegistroUSB registros[HISTORICO_MAX];
int registroAtual = -1;
uint32_t idAtual = 0, maiorId = 0, idAquisicao = 0, idEnvio = 0;
const char *comandoAtual = nullptr;
char dispositivo[24], sessao[17];
char bufferComando[RX_MAX + 1];
size_t tamanhoComando = 0;
bool descartarLinha = false;
unsigned long ultimoByteRX = 0;
char filaTX[TX_MAX];
size_t inicioTX = 0, tamanhoTX = 0;
uint32_t amostrasDescartadasUSB = 0, mensagensDescartadasUSB = 0;

size_t espacoTX() { return TX_MAX - tamanhoTX; }

bool enfileirar(const char *texto, bool amostra = false) {
  size_t n = strlen(texto);
  if (n + 1 + (amostra ? 4096 : 0) > espacoTX()) {
    if (amostra) amostrasDescartadasUSB++;
    else mensagensDescartadasUSB++;
    return false;
  }
  for (size_t i = 0; i < n; i++) filaTX[(inicioTX + tamanhoTX++) % TX_MAX] = texto[i];
  filaTX[(inicioTX + tamanhoTX++) % TX_MAX] = '\n';
  return true;
}

void atenderSaidaUSB() {
  int disponivel = Serial.availableForWrite();
  if (disponivel <= 0 || !tamanhoTX) return;
  size_t n = tamanhoTX;
  if (n > 64) n = 64;
  if (n > (size_t)disponivel) n = (size_t)disponivel;
  if (n > TX_MAX - inicioTX) n = TX_MAX - inicioTX;
  size_t enviados = Serial.write((const uint8_t *)filaTX + inicioTX, n);
  inicioTX = (inicioTX + enviados) % TX_MAX;
  tamanhoTX -= enviados;
}

int buscarRegistro(uint32_t id) {
  if (!id) return -1;
  for (uint8_t i = 0; i < HISTORICO_MAX; i++) if (registros[i].id == id) return i;
  return -1;
}

const char *estadoAtual() {
  if (ensaioAtivo) return "ADQUIRINDO";
  if (enviandoGravado) return "ENVIANDO_GRAVADO";
  return "OCIOSO";
}

void prepararMensagem(JsonDocument &doc, const char *tipo) {
  doc["protocolo"] = VERSAO_PROTOCOLO;
  doc["tipo"] = tipo;
  doc["sessao"] = sessao;
  doc["ms"] = millis();
  if (idAtual || comandoAtual) doc["id"] = idAtual;
  else doc["id"] = nullptr;
  if (comandoAtual) doc["comando"] = comandoAtual;
}

void publicar(JsonDocument &doc) {
  char texto[RESPOSTA_MAX];
  if (doc.overflowed() || measureJson(doc) >= sizeof(texto)) {
    enfileirar("{\"protocolo\":2,\"tipo\":\"erro\",\"id\":null,\"codigo\":\"JSON_MEMORIA\",\"origem\":\"ESP32\",\"descricao\":\"Resposta excede memoria disponivel.\"}");
    return;
  }
  serializeJson(doc, texto, sizeof(texto));
  // A resposta original fica disponivel mesmo se a saida USB estiver cheia.
  if (registroAtual >= 0 && (doc["tipo"] == "resposta" || doc["tipo"] == "erro")) {
    strcpy(registros[registroAtual].resposta, texto);
  }
  enfileirar(texto, doc["tipo"] == "amostra");
}

void emitirErro(const char *codigo, const char *origem, const char *descricao) {
  JsonDocument doc;
  prepararMensagem(doc, "erro");
  doc["ok"] = false;
  doc["fase"] = comandoAtual ? "rejeitado" : "falha";
  doc["nivel"] = "ERRO";
  doc["codigo"] = codigo;
  doc["origem"] = origem;
  doc["descricao"] = descricao;
  publicar(doc);
}

void emitirEvento(const char *codigo, const char *descricao) {
  JsonDocument doc;
  prepararMensagem(doc, "evento");
  doc["codigo"] = codigo;
  doc["descricao"] = descricao;
  doc["estado"] = estadoAtual();
  publicar(doc);
}

void concluirOperacao(uint32_t id, const char *codigo, const char *fase, const char *descricao) {
  JsonDocument doc;
  prepararMensagem(doc, "evento");
  doc.remove("comando");
  doc["id"] = id;
  if (idAtual) doc["causado_por"] = idAtual;
  doc["codigo"] = codigo;
  doc["fase"] = fase;
  doc["descricao"] = descricao;
  doc["estado"] = estadoAtual();
  doc["amostras_gravadas"] = amostrasGravadas;
  doc["amostras_enviadas"] = amostrasEnviadas;
  int i = buscarRegistro(id);
  if (i >= 0) {
    registros[i].pendente = false;
    if (!doc.overflowed() && measureJson(doc) < sizeof(registros[i].conclusao)) {
      serializeJson(doc, registros[i].conclusao, sizeof(registros[i].conclusao));
    }
  }
  publicar(doc);
}

void responderOK(bool demorada) {
  JsonDocument doc;
  prepararMensagem(doc, "resposta");
  doc["ok"] = true;
  doc["fase"] = demorada ? "aceito" : "concluido";
  doc["estado"] = estadoAtual();
  doc["lora_ao_vivo"] = transmitirLoRa;
  publicar(doc);
}

void informarIdentidade() {
  JsonDocument doc;
  prepararMensagem(doc, "resposta");
  doc["ok"] = true;
  doc["fase"] = "concluido";
  doc["dispositivo"] = dispositivo;
  doc["modelo"] = "Bancada_Empuxo_ESP32";
  doc["firmware"] = VERSAO_FIRMWARE;
  doc["baud"] = 115200;
  doc["maior_id"] = maiorId;
  doc["rx_max_bytes"] = RX_MAX;
  doc["rx_timeout_ms"] = RX_TIMEOUT_MS;
  doc["respostas_retidas"] = HISTORICO_MAX;
  publicar(doc);
}

void informarEstado() {
  JsonDocument doc;
  prepararMensagem(doc, "resposta");
  doc["ok"] = true;
  doc["fase"] = "concluido";
  doc["dispositivo"] = dispositivo;
  doc["firmware"] = VERSAO_FIRMWARE;
  doc["estado"] = estadoAtual();
  doc["aquisicao"]["ativa"] = ensaioAtivo;
  doc["aquisicao"]["id"] = idAquisicao;
  doc["aquisicao"]["amostras"] = amostrasGravadas;
  doc["sensor"]["respondendo"] = hx711Respondendo && millis() - ultimaLeituraHX < TIMEOUT_HX711_MS;
  doc["sensor"]["tara_pronta"] = taraPronta;
  doc["sensor"]["tara_em_andamento"] = taraInicialAtiva;
  doc["calibracao"]["unidade"] = "kgf";
  doc["calibracao"]["coeficiente"] = payload.fatorCalibracao;
  doc["calibracao"]["origem"] = "firmware";
  doc["calibracao"]["validacao_fisica"] = "nao_informada";
  doc["armazenamento"]["pronto"] = memoriaPronta;
  doc["armazenamento"]["tipo"] = "LittleFS_interno";
  doc["armazenamento"]["limite_ensaios"] = 1;
  doc["radio"]["pronto"] = loraPronto;
  doc["radio"]["tx"] = (bool)loraTransmitindo;
  doc["radio"]["ao_vivo"] = transmitirLoRa;
  doc["radio"]["envio_gravado"] = enviandoGravado;
  doc["radio"]["id_envio"] = idEnvio;
  doc["radio"]["enviadas"] = amostrasEnviadas;
  doc["radio"]["total"] = amostrasParaEnviar;
  doc["usb"]["amostras_descartadas"] = amostrasDescartadasUSB;
  doc["usb"]["mensagens_descartadas"] = mensagensDescartadasUSB;
  doc["usb"]["maior_id"] = maiorId;
  publicar(doc);
}

void informarAjuda() {
  JsonDocument doc;
  prepararMensagem(doc, "resposta");
  doc["ok"] = true;
  doc["fase"] = "concluido";
  doc["comandos"] = "IDENTIFICAR,STATUS,AJUDA,INICIAR,INICIAR_LORA,FINALIZAR,LORA_LIGAR,LORA_DESLIGAR,LORA_ALTERNAR,ENVIAR,CANCELAR";
  publicar(doc);
}

void informarAmostra() {
  JsonDocument doc;
  doc["protocolo"] = VERSAO_PROTOCOLO;
  doc["tipo"] = "amostra";
  doc["ensaio_id"] = idAquisicao;
  doc["tempo_ms"] = payload.tempo_ms;
  doc["empuxo_kgf"] = payload.empuxo;
  doc["indice"] = amostrasGravadas;
  publicar(doc);
}

// ---------------------------- HX711 E LORA ---------------------------------


unsigned long inicioTara = 0;
uint8_t leiturasTara = 0;
int64_t somaTara = 0;
unsigned long inicioPacoteLoRa = 0;

void atualizarTaraInicial() {
  if (!taraInicialAtiva) return;
  if (millis() - inicioTara >= TIMEOUT_TARA_MS) {
    taraInicialAtiva = false;
    emitirErro("HX711_TARA_TIMEOUT", "HX711", "Tara inicial incompleta em 3000 ms; reinicie sem carga.");
  } else if (celula.is_ready()) {
    somaTara += celula.read();
    if (++leiturasTara == 10) {
      celula.set_offset((long)(somaTara / 10));
      taraInicialAtiva = false;
      taraPronta = hx711Respondendo = true;
      ultimaLeituraHX = millis();
      emitirEvento("TARA_CONCLUIDA", "Tara inicial concluida.");
    }
  }
}

// Callback publico da biblioteca: nao faz SPI, Serial ou gravacao na interrupcao.
void aoConcluirTransmissaoLoRa() {
  loraTransmitindo = false;
}

size_t montarJson(const Payload &p, char *buffer, size_t tamanho) {
  JsonDocument doc;
  doc["tempo_ms"] = p.tempo_ms;
  doc["empuxo"] = p.empuxo;
  doc["fator_calibracao"] = p.fatorCalibracao;
  if (doc.overflowed() || measureJson(doc) >= tamanho) return 0;
  return serializeJson(doc, buffer, tamanho);
}

// 1 = pacote entregue ao radio; 0 = ocupado (tentar depois); -1 = falha.
// Nenhum retorno confirma que uma estacao receptora recebeu o pacote.
int transmitirPayload(const Payload &p) {
  char json[128];
  size_t tamanho = montarJson(p, json, sizeof(json));
  if (tamanho == 0) {
    emitirErro("LORA_JSON", "LoRa", "Nao foi possivel montar o pacote LoRa.");
    return -1;
  }
  if (loraTransmitindo || !LoRa.beginPacket()) return 0;
  if (LoRa.print(json) != tamanho) {
    emitirErro("LORA_ENVIO", "LoRa", "Falha ao preparar o pacote.");
    return -1;
  }
  // Definir antes de endPacket evita perder uma conclusao rapida.
  inicioPacoteLoRa = millis();
  loraTransmitindo = true;
  if (LoRa.endPacket(true) != 1) {
    loraTransmitindo = false;
    emitirErro("LORA_ENVIO", "LoRa", "Falha ao iniciar a transmissao do pacote.");
    return -1;
  }
  return 1;
}

void atualizarRitmoEnvio(unsigned long agora) {
  ultimoEnvio += INTERVALO_ENVIO_MS;
  if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) ultimoEnvio = agora;
}

void encerrarEnsaio(const char *motivo) {
  ensaioAtivo = false;
  transmitirLoRa = false;
  arquivoEnsaio.close();
  digitalWrite(LED_EXEC, LOW);
  concluirOperacao(idAquisicao, "ENSAIO_ENCERRADO", idAtual ? "concluido" : "falhou", motivo);
  idAquisicao = 0;
}

void encerrarEnvio(const char *codigo, const char *mensagem) {
  arquivoEnsaio.close();
  enviandoGravado = false;
  const char *fase = !strcmp(codigo, "ENVIO_CONCLUIDO") ? "concluido" :
                     !strcmp(codigo, "ENVIO_CANCELADO") ? "cancelado" : "falhou";
  concluirOperacao(idEnvio, codigo, fase, mensagem);
  idEnvio = 0;
}

// ------------------------------- ACOES ------------------------------------

bool acaoIniciar() {
  if (ensaioAtivo) {
    emitirErro("ENSAIO_ATIVO", "ESP32", "Ja existe um ensaio em andamento.");
    return false;
  }
  if (enviandoGravado) {
    emitirErro("ENVIO_ATIVO", "ESP32", "Aguarde o envio terminar ou use CANCELAR.");
    return false;
  }
  if (!memoriaPronta) {
    emitirErro("FLASH_INDISPONIVEL", "Flash", "Memoria flash indisponivel.");
    return false;
  }
  if (!taraPronta || !hx711Respondendo ||
      millis() - ultimaLeituraHX >= TIMEOUT_HX711_MS) {
    emitirErro("HX711_INDISPONIVEL", "HX711", "Confira o sensor e reinicie sem carga para refazer a tara inicial.");
    return false;
  }
  if (!isfinite(payload.fatorCalibracao) || payload.fatorCalibracao == 0.0) {
    emitirErro("COEFICIENTE_INVALIDO", "ESP32", "O coeficiente deve ser finito e diferente de zero.");
    return false;
  }
  arquivoEnsaio = LittleFS.open(ARQUIVO_ENSAIO, FILE_WRITE);
  if (!arquivoEnsaio) {
    emitirErro("FLASH_ABERTURA", "Flash", "Nao foi possivel criar o arquivo do ensaio.");
    return false;
  }
  amostrasGravadas = 0;
  amostrasParaEnviar = 0;
  amostrasEnviadas = 0;
  inicioEnsaio = millis();
  ultimoEnvio = inicioEnsaio;
  transmitirLoRa = false;
  temLeitura = false;
  ensaioAtivo = true;
  digitalWrite(LED_EXEC, HIGH);
  return true;
}

bool acaoIniciarComLoRa() {
  // Falha sem iniciar/apagar um ensaio quando o radio nao esta disponivel.
  if (!loraPronto) {
    emitirErro("LORA_INDISPONIVEL", "LoRa", "LoRa indisponivel; use INICIAR para gravar sem radio.");
    return false;
  }
  if (!acaoIniciar()) return false;
  return acaoLigarLoRa();
}

bool acaoFinalizar() {
  if (ensaioAtivo) encerrarEnsaio("Finalizado por comando.");
  return true; // repetir FINALIZAR nao inicia outra operacao
}

bool acaoLigarLoRa() {
  if (!ensaioAtivo) {
    emitirErro("ENSAIO_INATIVO", "ESP32", "Inicie um ensaio antes de ligar o LoRa ao vivo.");
    return false;
  }
  if (!loraPronto) {
    emitirErro("LORA_INDISPONIVEL", "LoRa", "LoRa indisponivel.");
    return false;
  }
  if (!transmitirLoRa) {
    transmitirLoRa = true;
    ultimoEnvio = millis() - INTERVALO_ENVIO_MS;
  }
  return true;
}

bool acaoDesligarLoRa() {
  transmitirLoRa = false;
  return true;
}

bool acaoEnviarGravado() {
  if (ensaioAtivo) {
    emitirErro("ENSAIO_ATIVO", "ESP32", "Finalize o ensaio antes de enviar a gravacao.");
    return false;
  }
  if (enviandoGravado) {
    emitirErro("ENVIO_ATIVO", "ESP32", "O ensaio gravado ja esta sendo enviado.");
    return false;
  }
  if (!loraPronto) {
    emitirErro("LORA_INDISPONIVEL", "LoRa", "LoRa indisponivel.");
    return false;
  }
  if (!memoriaPronta) {
    emitirErro("FLASH_INDISPONIVEL", "Flash", "Memoria flash indisponivel.");
    return false;
  }
  arquivoEnsaio = LittleFS.open(ARQUIVO_ENSAIO, FILE_READ);
  if (!arquivoEnsaio || arquivoEnsaio.size() == 0) {
    if (arquivoEnsaio) arquivoEnsaio.close();
    emitirErro("SEM_ENSAIO", "Flash", "Nenhum ensaio gravado.");
    return false;
  }
  if (arquivoEnsaio.size() % sizeof(Payload) != 0) {
    arquivoEnsaio.close();
    emitirErro("FLASH_FORMATO", "Flash", "O arquivo tem uma amostra incompleta.");
    return false;
  }
  amostrasParaEnviar = arquivoEnsaio.size() / sizeof(Payload);
  amostrasEnviadas = 0;
  ultimoEnvio = millis() - INTERVALO_ENVIO_MS;
  enviandoGravado = true;
  return true;
}

bool acaoCancelarEnvio() {
  if (enviandoGravado) encerrarEnvio("ENVIO_CANCELADO", "Envio cancelado por comando.");
  return true;
}

// ------------------------ RECEPCAO JSON POR LINHA --------------------------
// Valida a sintaxe estrita do envelope plano antes do ArduinoJson.
// Evita extensoes permissivas, chaves repetidas e lixo depois do objeto.
void pularEspacos(const char *&p) { while (*p == ' ' || *p == '\t' || *p == '\r') p++; }

bool pularStringJSON(const char *&p) {
  if (*p++ != '"') return false;
  while (*p && *p != '"') {
    if ((unsigned char)*p < 32) return false;
    if (*p++ == '\\') {
      if (!*p) return false;
      if (*p == 'u') {
        p++;
        for (uint8_t i = 0; i < 4; i++) {
          if (!*p || !isxdigit((unsigned char)*p)) return false;
          p++;
        }
      } else {
        if (!strchr("\"\\/bfnrt", *p)) return false;
        p++;
      }
    }
  }
  if (*p != '"') return false;
  p++;
  return true;
}

bool envelopeEstrito(const char *p, size_t &campos) {
  campos = 0;
  pularEspacos(p);
  if (*p++ != '{') return false;
  pularEspacos(p);
  if (*p == '}') { p++; pularEspacos(p); return !*p; }
  while (*p) {
    if (*p != '"' || !pularStringJSON(p)) return false;
    pularEspacos(p);
    if (*p++ != ':') return false;
    pularEspacos(p);
    if (*p == '"') { if (!pularStringJSON(p)) return false; }
    else {
      // Os campos numericos do envelope sao inteiros sem sinal.
      if (!isdigit((unsigned char)*p)) return false;
      if (*p == '0') p++;
      else while (isdigit((unsigned char)*p)) p++;
    }
    campos++;
    pularEspacos(p);
    if (*p == '}') { p++; pularEspacos(p); return !*p; }
    if (*p++ != ',') return false;
    pularEspacos(p);
  }
  return false;
}

void processarComando(const char *linha) {
  size_t campos;
  JsonDocument pedido;
  if (!envelopeEstrito(linha, campos)) {
    emitirErro("JSON_INVALIDO", "USB", "Esperado um objeto JSON plano, completo e sem texto adicional.");
    return;
  }
  DeserializationError erro = deserializeJson(pedido, linha, DeserializationOption::NestingLimit(2));
  if (erro || !pedido.is<JsonObject>() || pedido.size() != campos) {
    emitirErro("JSON_INVALIDO", "USB", "JSON invalido, chaves repetidas ou memoria insuficiente.");
    return;
  }
  if (!pedido["id"].is<uint32_t>()) {
    emitirErro("ID_INVALIDO", "USB", "id deve ser inteiro de 0 a 4294967295.");
    return;
  }
  idAtual = pedido["id"].as<uint32_t>();
  // Escopo restaura contexto inclusive nos retornos por validacao ou repeticao.
  struct LimparContexto {
    ~LimparContexto() { idAtual = 0; comandoAtual = nullptr; registroAtual = -1; }
  } limpar;
  if (!pedido["comando"].is<const char *>()) {
    emitirErro("COMANDO_INVALIDO", "USB", "comando deve ser uma string."); return;
  }
  JsonString nome = pedido["comando"].as<JsonString>();
  if (!nome.size() || nome.size() > 24 || strlen(nome.c_str()) != nome.size()) {
    emitirErro("COMANDO_INVALIDO", "USB", "Nome deve ter 1 a 24 caracteres ASCII."); return;
  }
  for (size_t i = 0; i < nome.size(); i++) {
    char c = nome.c_str()[i];
    if ((c < 'A' || c > 'Z') && c != '_') {
      emitirErro("COMANDO_INVALIDO", "USB", "Use nome em maiusculas, sem atalhos."); return;
    }
  }
  comandoAtual = nome.c_str();
  if (!pedido["protocolo"].is<unsigned int>() || pedido["protocolo"].as<unsigned int>() != VERSAO_PROTOCOLO) {
    emitirErro("PROTOCOLO_INVALIDO", "USB", "Esta versao exige protocolo 2."); return;
  }
  if (pedido["tipo"] != "comando") {
    emitirErro("TIPO_INVALIDO", "USB", "tipo deve ser comando."); return;
  }
  for (JsonPair campo : pedido.as<JsonObject>()) {
    const char *k = campo.key().c_str();
    if (strlen(k) != campo.key().size() || (strcmp(k, "id") && strcmp(k, "comando") &&
        strcmp(k, "tipo") && strcmp(k, "protocolo") && strcmp(k, "sessao"))) {
      emitirErro("CAMPO_DESCONHECIDO", "USB", "Envelope possui campo nao suportado."); return;
    }
  }
  if (!idAtual && !strcmp(comandoAtual, "IDENTIFICAR") && pedido.size() == 4) {
    informarIdentidade(); return;
  }
  if (!idAtual) { emitirErro("ID_INVALIDO", "USB", "id 0 e reservado a descoberta IDENTIFICAR sem sessao."); return; }
  if (!pedido["sessao"].is<const char *>() || pedido["sessao"].as<JsonString>().size() != 16 ||
      strcmp(pedido["sessao"].as<const char *>(), sessao)) {
    emitirErro("SESSAO_INVALIDA", "USB", "Consulte IDENTIFICAR com id 0 e use a sessao atual."); return;
  }
  int anterior = buscarRegistro(idAtual);
  if (anterior >= 0) {
    if (strcmp(registros[anterior].comando, comandoAtual)) {
      emitirErro("ID_CONFLITO", "USB", "Este id ja pertence a outro comando."); return;
    }
    if (registros[anterior].resposta[0]) enfileirar(registros[anterior].resposta);
    if (registros[anterior].conclusao[0]) enfileirar(registros[anterior].conclusao);
    return;
  }
  if (idAtual <= maiorId) {
    emitirErro("ID_EXPIRADO", "USB", "ID antigo sem resposta retida; acao nao foi repetida. Consulte STATUS com novo id."); return;
  }
  int escolhido = -1;
  for (uint8_t i = 0; i < HISTORICO_MAX; i++) {
    if (!registros[i].pendente && (escolhido < 0 || registros[i].id < registros[escolhido].id)) escolhido = i;
  }
  if (escolhido < 0) { emitirErro("REQUISICOES_OCUPADAS", "USB", "Sem espaco para registrar solicitacao."); return; }
  registroAtual = escolhido;
  registros[escolhido] = RegistroUSB();
  registros[escolhido].id = maiorId = idAtual;
  strcpy(registros[escolhido].comando, comandoAtual);
  bool ok = false, demorada = false;
  if (!strcmp(comandoAtual, "IDENTIFICAR")) { informarIdentidade(); return; }
  if (!strcmp(comandoAtual, "STATUS")) { informarEstado(); return; }
  if (!strcmp(comandoAtual, "AJUDA")) { informarAjuda(); return; }
  if (!strcmp(comandoAtual, "INICIAR")) { ok = opcoes.iniciar(); demorada = true; }
  else if (!strcmp(comandoAtual, "INICIAR_LORA")) { ok = opcoes.iniciarComLoRa(); demorada = true; }
  else if (!strcmp(comandoAtual, "FINALIZAR")) ok = opcoes.finalizar();
  else if (!strcmp(comandoAtual, "LORA_LIGAR")) ok = opcoes.ligarLoRa();
  else if (!strcmp(comandoAtual, "LORA_DESLIGAR")) ok = opcoes.desligarLoRa();
  else if (!strcmp(comandoAtual, "LORA_ALTERNAR")) ok = transmitirLoRa ? opcoes.desligarLoRa() : opcoes.ligarLoRa();
  else if (!strcmp(comandoAtual, "ENVIAR")) { ok = opcoes.enviarGravado(); demorada = true; }
  else if (!strcmp(comandoAtual, "CANCELAR")) ok = opcoes.cancelarEnvio();
  else emitirErro("COMANDO_DESCONHECIDO", "USB", "Comando nao suportado. Consulte AJUDA.");
  if (ok) {
    registros[registroAtual].pendente = demorada;
    if (demorada) {
      if (enviandoGravado) idEnvio = idAtual;
      else idAquisicao = idAtual;
    }
    responderOK(demorada);
    emitirEvento("ESTADO_ATUALIZADO", demorada ? "Operacao iniciada; conclusao sera informada por evento." : "Comando concluido.");
  }
}

void ReceberComando() {
  if (!descartarLinha && tamanhoComando && millis() - ultimoByteRX >= RX_TIMEOUT_MS) {
    descartarLinha = true;
    tamanhoComando = 0;
    emitirErro("RX_TIMEOUT", "USB", "Linha incompleta; descartando ate o proximo LF.");
  }
  // Reserva espaco para resposta e eventos antes de aceitar outra operacao.
  if (espacoTX() < 4096) return;
  for (uint8_t i = 0; i < 64 && Serial.available(); i++) {
    char c = (char)Serial.read();
    ultimoByteRX = millis();
    if (c == '\n') {
      if (!descartarLinha && tamanhoComando) {
        bufferComando[tamanhoComando] = '\0';
        const char *p = bufferComando; pularEspacos(p);
        if (*p) processarComando(bufferComando);
      }
      tamanhoComando = 0;
      descartarLinha = false;
      return;
    }
    if (descartarLinha) continue;
    if (((unsigned char)c < 32 && c != '\t' && c != '\r') || tamanhoComando >= RX_MAX) {
      emitirErro(tamanhoComando >= RX_MAX ? "RX_TAMANHO" : "RX_CARACTERE", "USB", "Linha descartada ate LF: tamanho ou caractere invalido.");
      tamanhoComando = 0;
      descartarLinha = true;
    } else bufferComando[tamanhoComando++] = c;
  }
}

// ----------------------------- SETUP / LOOP -------------------------------

void setup() {
  Serial.begin(115200);
  snprintf(dispositivo, sizeof(dispositivo), "ESP32-%012llX", (unsigned long long)ESP.getEfuseMac());
  snprintf(sessao, sizeof(sessao), "%08lX%08lX", (unsigned long)esp_random(), (unsigned long)esp_random());
  pinMode(LED_EXEC, OUTPUT);
  pinMode(LED_LORA, OUTPUT);
  digitalWrite(LED_EXEC, LOW);
  digitalWrite(LED_LORA, LOW);
  emitirEvento("BOOT", "Bancada V0.13 iniciando.");

  memoriaPronta = LittleFS.begin(true); // politica de montagem preservada
  if (!memoriaPronta) emitirErro("FLASH_INICIALIZACAO", "Flash", "Falha ao montar a flash interna.");

  SPI.begin(18, 19, 23, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  LoRa.setSPIFrequency(1000000);
  loraPronto = LoRa.begin(915E6);
  if (loraPronto) {
    LoRa.setSyncWord(145);
    LoRa.setTxPower(2);
    LoRa.onTxDone(aoConcluirTransmissaoLoRa);
  } else {
    emitirErro("LORA_INICIALIZACAO", "LoRa", "Falha ao iniciar o LoRa.");
  }

  celula.begin(PINO_DT, PINO_SCK);
  taraInicialAtiva = true;
  inicioTara = millis();
  if (FATOR_CALIBRACAO_PADRAO == 1.0) {
    JsonDocument doc;
    prepararMensagem(doc, "aviso");
    doc["nivel"] = "AVISO";
    doc["origem"] = "ESP32";
    doc["codigo"] = "COEFICIENTE_PADRAO";
    doc["descricao"] = "Coeficiente 1.0 preservado da V0.9. Valores nao representam kgf validados ate calibrar.";
    publicar(doc);
  }
  emitirEvento("USB_PRONTA", "Use IDENTIFICAR com id 0; tara inicial em andamento.");
}

void loop() {
  atenderSaidaUSB();
  ReceberComando();
  atualizarTaraInicial();
  if (taraInicialAtiva) return;
  if (loraTransmitindo && millis() - inicioPacoteLoRa >= 5000) {
    LoRa.idle();
    loraTransmitindo = false;
    loraPronto = transmitirLoRa = false;
    emitirErro("LORA_TX_TIMEOUT", "LoRa", "Sem TX_DONE em 5000 ms; radio desabilitado ate reiniciar.");
    if (enviandoGravado) encerrarEnvio("ENVIO_INTERROMPIDO", "Sem confirmacao local do fim de TX.");
    else emitirEvento("ESTADO_ATUALIZADO", "Envio ao vivo desabilitado por timeout do radio.");
  }
  digitalWrite(LED_EXEC, ensaioAtivo);
  // O amarelo acompanha a atividade de TX local, inclusive o ultimo pacote.
  digitalWrite(LED_LORA, loraPronto && loraTransmitindo);

  if (celula.is_ready()) {
    long leitura = (long)celula.get_value();
    ultimaLeituraHX = millis();
    hx711Respondendo = true;
    if (ensaioAtivo) {
      payload.tempo_ms = millis() - inicioEnsaio;
      payload.empuxo = (double)leitura / payload.fatorCalibracao;
      if (!isfinite(payload.empuxo)) {
        emitirErro("MEDICAO_INVALIDA", "HX711", "O calculo de empuxo produziu um valor nao finito.");
        encerrarEnsaio("Interrompido por erro no calculo do empuxo.");
      } else if (arquivoEnsaio.write((const uint8_t *)&payload, sizeof(Payload)) == sizeof(Payload)) {
        temLeitura = true;
        amostrasGravadas++;
        informarAmostra();
      } else {
        emitirErro("FLASH_GRAVACAO", "Flash", "Falha ao gravar uma amostra; pode ser falta de espaco.");
        encerrarEnsaio("Interrompido por falha de gravacao.");
      }
    }
  } else if (hx711Respondendo && millis() - ultimaLeituraHX >= TIMEOUT_HX711_MS) {
    hx711Respondendo = false;
    taraPronta = false;
    emitirErro("HX711_TIMEOUT", "HX711", "Sem novas leituras ha 2000 ms. Reinicie sem carga para refazer a tara.");
    if (ensaioAtivo) encerrarEnsaio("Interrompido por ausencia de leituras do HX711.");
  }

  if (ensaioAtivo && loraPronto && transmitirLoRa && temLeitura) {
    unsigned long agora = millis();
    if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
      int resultado = transmitirPayload(payload);
      if (resultado > 0) atualizarRitmoEnvio(agora);
      else if (resultado < 0) transmitirLoRa = false;
    }
  }

  if (enviandoGravado) {
    unsigned long agora = millis();
    if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
      if (amostrasEnviadas >= amostrasParaEnviar) {
        if (!loraTransmitindo) encerrarEnvio("ENVIO_CONCLUIDO", "Todos os pacotes foram transmitidos localmente; recepcao nao confirmada.");
      } else {
        size_t posicao = arquivoEnsaio.position();
        Payload gravado;
        if (arquivoEnsaio.read((uint8_t *)&gravado, sizeof(Payload)) != sizeof(Payload)) {
          emitirErro("FLASH_LEITURA", "Flash", "Falha ao ler uma amostra do ensaio gravado.");
          encerrarEnvio("ENVIO_INTERROMPIDO", "Interrompido por falha de leitura.");
        } else {
          int resultado = transmitirPayload(gravado);
          if (resultado > 0) {
            amostrasEnviadas++;
            atualizarRitmoEnvio(agora);
          } else if (resultado < 0) {
            encerrarEnvio("ENVIO_INTERROMPIDO", "Interrompido por falha no envio LoRa.");
          } else if (!arquivoEnsaio.seek(posicao)) {
            emitirErro("FLASH_POSICAO", "Flash", "Falha ao reposicionar a leitura para tentar novamente.");
            encerrarEnvio("ENVIO_INTERROMPIDO", "Interrompido por falha de acesso ao arquivo.");
          }
        }
      }
    }
  }
}
