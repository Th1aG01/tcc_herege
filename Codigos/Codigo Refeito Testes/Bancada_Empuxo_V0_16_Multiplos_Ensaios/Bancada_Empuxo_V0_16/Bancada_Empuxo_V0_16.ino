// Bancada de Empuxo V0.16 - Protocolo USB JSON 2 e diagnosticos.
// Base: V0.15. USB: objetos JSON completos terminados por LF (protocolo 2).
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
#include <stdlib.h>
#include <string.h>

const char *VERSAO_FIRMWARE = "0.16";
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

// Coeficiente em contagens/kgf; valor inicial bloqueado ate configuracao USB explicita.
// 1.0 e o valor original de demonstracao, NAO uma calibracao fisica validada.
const double FATOR_CALIBRACAO_PADRAO = 1.0;
char caminhoGravacao[64] = {}, caminhoTransmissao[64] = {}, idTransmissao[32] = {};
char idPacoteLoRa[32] = {};
int32_t taraReferencia = 0;
uint32_t revisaoCatalogo = 1;
JsonDocument *pedidoAtual = nullptr;
const unsigned long TIMEOUT_HX711_MS = 2000;
const unsigned long TIMEOUT_TARA_MS = 3000;


#include "Armazenamento.h"

struct ResumoEnsaio {
  char id[32] = {};
  uint32_t solicitacao = 0;
  uint32_t duracao_ms = 0;
  uint32_t amostras = 0;
  double coeficiente = 0;
  bool iniciado = false;
  bool fechamentoConfirmado = false;
  const char *resultado = "NENHUM";
  char motivo[32] = {};
};
ResumoEnsaio resumo;
uint32_t contadorEnsaios = 0, ultimoIndiceLoRa = 0;
bool aquisicaoIniciada = false;
CabecalhoEnsaio cabecalhoEnvio = {}, cabecalhoGravacao = {};
unsigned long ultimoFlush = 0;

bool exclusaoTotalAtiva();
void adicionarEstadoCatalogo(JsonDocument &doc);
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
bool calibracaoConfigurada = false;
bool taraPronta = false;
uint32_t idTara = 0;
unsigned long inicioTara = 0;
uint8_t leiturasTara = 0;
int64_t somaTara = 0;
bool taraEmAndamento = false;
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
Payload payload = {0, 0, 0.0, FATOR_CALIBRACAO_PADRAO, {}};
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
  double coeficiente = 0; // parametro normalizado para detectar ID_CONFLITO
  char parametros[513] = {};
  bool pendente = false;
  char resposta[RESPOSTA_MAX] = {};
  char conclusao[RESPOSTA_MAX] = {};
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
  if (ensaioAtivo) return aquisicaoIniciada ? "ADQUIRINDO" : "PREPARANDO";
  if (enviandoGravado) return "ENVIANDO_GRAVADO";
  if (taraEmAndamento) return "TARANDO";
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

void emitirErro(const char *codigo, const char *origem, const char *descricao, bool falha = false) {
  JsonDocument doc;
  prepararMensagem(doc, "erro");
  doc["ok"] = false;
  doc["fase"] = falha || !comandoAtual ? "falha" : "rejeitado";
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

bool calibracaoValida() {
  return calibracaoConfigurada && isfinite(payload.fatorCalibracao) && payload.fatorCalibracao != 0.0;
}

void adicionarCalibracao(JsonDocument &doc) {
  bool sensorRecente = hx711Respondendo && millis() - ultimaLeituraHX < TIMEOUT_HX711_MS;
  doc["calibracao"]["unidade"] = "kgf";
  doc["calibracao"]["coeficiente"] = payload.fatorCalibracao;
  doc["calibracao"]["configurada"] = calibracaoConfigurada;
  doc["calibracao"]["valida"] = calibracaoValida();
  doc["calibracao"]["origem"] = calibracaoConfigurada ? "comando_usb" : "padrao_inicial";
  doc["calibracao"]["validacao_fisica"] = "responsabilidade_do_operador";
  doc["calibracao"]["pronta_para_uso"] = calibracaoValida() && taraPronta && !taraEmAndamento && sensorRecente;
  doc["tara"]["pronta"] = taraPronta;
  doc["tara"]["em_andamento"] = taraEmAndamento;
  doc["tara"]["leituras"] = leiturasTara;
  doc["tara"]["necessarias"] = 10;
  doc["tara"]["timeout_ms"] = TIMEOUT_TARA_MS;
}

void consultarCalibracao() {
  JsonDocument doc;
  prepararMensagem(doc, "resposta");
  doc["ok"] = true;
  doc["fase"] = "concluido";
  adicionarCalibracao(doc);
  publicar(doc);
}

uint32_t duracaoEnsaio() {
  return ensaioAtivo && aquisicaoIniciada ? (uint32_t)(millis() - inicioEnsaio) : resumo.duracao_ms;
}

void adicionarEnsaio(JsonDocument &doc) {
  doc["ensaio"]["id"] = resumo.id[0] ? resumo.id : nullptr;
  doc["ensaio"]["solicitacao_id"] = resumo.solicitacao;
  doc["ensaio"]["coeficiente"] = resumo.coeficiente;
  doc["ensaio"]["iniciado"] = resumo.iniciado;
  doc["ensaio"]["resultado"] = resumo.resultado;
  doc["ensaio"]["motivo"] = resumo.motivo;
  doc["ensaio"]["duracao_ms"] = duracaoEnsaio();
  doc["ensaio"]["amostras"] = resumo.amostras;
  doc["ensaio"]["fechamento_confirmado"] = resumo.fechamentoConfirmado;
  // Apos reset sem rodape, contagem e duracao nao foram recuperadas.
  if (!strcmp(resumo.resultado, "INCOMPLETO")) {
    doc["ensaio"]["amostras"] = nullptr;
    doc["ensaio"]["duracao_ms"] = nullptr;
    doc["ensaio"]["iniciado"] = nullptr;
    doc["ensaio"]["coeficiente"] = nullptr;
  }
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
  if (id == idAquisicao && idAquisicao) adicionarEnsaio(doc);
  doc["amostras_gravadas"] = amostrasGravadas;
  doc["amostras_enviadas"] = amostrasEnviadas;
  adicionarCalibracao(doc);
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
  adicionarEnsaio(doc);
  adicionarCalibracao(doc);
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
  doc["formato_amostra"] = 2;
  doc["formato_gravacao"] = FORMATO_GRAVACAO;
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
  doc["aquisicao"]["ativa"] = ensaioAtivo && aquisicaoIniciada;
  doc["aquisicao"]["preparando"] = ensaioAtivo && !aquisicaoIniciada;
  adicionarEnsaio(doc);
  doc["aquisicao"]["id"] = idAquisicao;
  doc["aquisicao"]["amostras"] = amostrasGravadas;
  doc["sensor"]["respondendo"] = hx711Respondendo && millis() - ultimaLeituraHX < TIMEOUT_HX711_MS;
  doc["sensor"]["tara_pronta"] = taraPronta;
  doc["sensor"]["tara_em_andamento"] = taraEmAndamento;
  adicionarCalibracao(doc);
  doc["armazenamento"]["pronto"] = memoriaPronta;
  doc["armazenamento"]["tipo"] = "LittleFS_interno";
  doc["armazenamento"]["catalogo_revisao"] = revisaoCatalogo;
  doc["armazenamento"]["multiplos_ensaios"] = true;
  doc["armazenamento"]["formato"] = FORMATO_GRAVACAO;
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
  adicionarEstadoCatalogo(doc);
  publicar(doc);
}

void informarAjuda() {
  JsonDocument doc;
  prepararMensagem(doc, "resposta");
  doc["ok"] = true;
  doc["fase"] = "concluido";
  doc["comandos"] = "IDENTIFICAR,STATUS,AJUDA,INICIAR,INICIAR_LORA,FINALIZAR,LORA_LIGAR,LORA_DESLIGAR,LORA_ALTERNAR,ENVIAR,CANCELAR,CONFIGURAR_CALIBRACAO,CONSULTAR_CALIBRACAO,TARA,LISTAR_ENSAIOS,CONSULTAR_ENSAIO,LER_AMOSTRAS,EXCLUIR_ENSAIOS,EXCLUIR_TODOS,ESPACO";
  publicar(doc);
}

// Uma unica representacao JSON da mesma amostra para USB e LoRa.
void montarAmostra(JsonDocument &doc, const Payload &p) {
  doc["protocolo"] = VERSAO_PROTOCOLO;
  doc["tipo"] = "amostra";
  doc["ensaio_id"] = p.ensaioId;
  doc["indice"] = p.indice;
  doc["tempo_ms"] = p.tempo_ms;
  doc["empuxo_kgf"] = p.empuxo;
}

void informarAmostra() {
  JsonDocument doc;
  montarAmostra(doc, payload);
  publicar(doc);
}

// ---------------------------- HX711 E LORA ---------------------------------


unsigned long inicioPacoteLoRa = 0;

void iniciarTara(uint32_t solicitacao) {
  idTara = solicitacao;
  taraEmAndamento = true;
  taraPronta = false;
  inicioTara = millis();
  leiturasTara = 0;
  somaTara = 0;
}

bool acaoTara() {
  if (ensaioAtivo) {
    emitirErro("ENSAIO_ATIVO", "HX711", "Finalize a aquisicao antes de executar tara.");
    return false;
  }
  if (taraEmAndamento) {
    emitirErro("TARA_EM_ANDAMENTO", "HX711", "Ja existe uma tara em andamento.");
    return false;
  }
  // Ausencia momentanea de DOUT pronto nao prova sensor desconectado.
  // A operacao verifica cada nova leitura e termina com sucesso ou timeout.
  iniciarTara(idAtual);
  return true;
}

void atualizarTara() {
  if (!taraEmAndamento) return;
  if (millis() - inicioTara >= TIMEOUT_TARA_MS) {
    taraEmAndamento = false;
    taraPronta = false;
    if (millis() - ultimaLeituraHX >= TIMEOUT_HX711_MS) hx711Respondendo = false;
    emitirErro(idTara ? "HX711_TARA_TIMEOUT" : "HX711_TARA_INICIAL_TIMEOUT", "HX711",
               "Nao chegaram dez leituras em 3000 ms. Confira o HX711 e envie TARA sem carga.");
    if (idTara) concluirOperacao(idTara, "TARA_FALHOU", "falhou", "Tara nao aplicada; sensor ausente ou leituras insuficientes.");
    else emitirEvento("TARA_FALHOU", "Tara inicial indisponivel; USB continua funcionando.");
    idTara = 0;
  } else if (celula.is_ready()) {
    // read() e bruto: nao subtrair a tara anterior ao calcular a nova.
    somaTara += celula.read();
    ultimaLeituraHX = millis();
    hx711Respondendo = true;
    if (++leiturasTara == 10) {
      taraReferencia = (int32_t)(somaTara / 10);
      celula.set_offset(taraReferencia);
      taraEmAndamento = false;
      taraPronta = true;
      if (idTara) concluirOperacao(idTara, "TARA_CONCLUIDA", "concluido", "Tara calculada e aplicada internamente ao HX711.");
      else emitirEvento("TARA_CONCLUIDA", "Tara inicial concluida.");
      idTara = 0;
    }
  }
}

bool acaoConfigurarCalibracao(double coeficiente) {
  if (ensaioAtivo) {
    emitirErro("ENSAIO_ATIVO", "Calibracao", "Finalize a aquisicao antes de alterar o coeficiente.");
    return false;
  }
  if (taraEmAndamento) {
    emitirErro("TARA_EM_ANDAMENTO", "Calibracao", "Aguarde a conclusao da tara antes de configurar o coeficiente.");
    return false;
  }
  payload.fatorCalibracao = coeficiente;
  calibracaoConfigurada = true;
  // Um novo perfil pode corresponder a outra celula: exige nova tara.
  // A repeticao do MESMO id e interceptada antes e nao invalida a tara.
  taraPronta = false;
  return true;
}

// Callback publico da biblioteca: nao faz SPI, Serial ou gravacao na interrupcao.
void aoConcluirTransmissaoLoRa() {
  loraTransmitindo = false;
}

size_t montarJson(const Payload &p, char *buffer, size_t tamanho) {
  JsonDocument doc;
  montarAmostra(doc, p);
  if (doc.overflowed() || measureJson(doc) >= tamanho) return 0;
  return serializeJson(doc, buffer, tamanho);
}

// 1 = pacote entregue ao radio; 0 = ocupado (tentar depois); -1 = falha.
// Nenhum retorno confirma que uma estacao receptora recebeu o pacote.
int transmitirPayload(const Payload &p) {
  char json[256];
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
  strcpy(idPacoteLoRa, p.ensaioId);
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

void encerrarEnsaio(const char *motivo, bool normal = false) {
  resumo.duracao_ms = duracaoEnsaio();
  ensaioAtivo = false;
  aquisicaoIniciada = false;
  transmitirLoRa = false;
  digitalWrite(LED_EXEC, LOW);
  bool sucesso = normal && resumo.iniciado;
  strcpy(resumo.motivo, normal && !resumo.iniciado ? "CANCELADO_ANTES_INICIO" : motivo);
  RodapeEnsaio fim = {};
  fim.magic = MAGIC_FIM;
  fim.amostraBytes = sizeof(RegistroAmostra);
  fim.resultado = sucesso ? 1 : 2;
  fim.amostras = amostrasGravadas;
  fim.duracao_ms = resumo.duracao_ms;
  strcpy(fim.motivo, resumo.motivo);
  fim.crc = crc32(&fim, offsetof(RodapeEnsaio, crc));
  // Amostra parcial impede gravar um marcador que pareca valido.
  bool completo = arquivoEnsaio && arquivoEnsaio.size() == sizeof(CabecalhoEnsaio) +
                  (uint64_t)amostrasGravadas * sizeof(RegistroAmostra);
  bool gravouFim = completo && arquivoEnsaio.write((const uint8_t *)&fim, sizeof(fim)) == sizeof(fim);
  arquivoEnsaio.flush(); // API void: confirmar tambem apos fechar e reabrir.
  arquivoEnsaio.close();
  File verificar = LittleFS.open(caminhoGravacao, FILE_READ);
  CabecalhoEnsaio cabLido = {};
  RodapeEnsaio fimLido = {};
  resumo.fechamentoConfirmado = gravouFim && lerFechamento(verificar, cabLido, fimLido) &&
    !strcmp(cabLido.ensaioId, resumo.id) && !memcmp(&fimLido, &fim, sizeof(fim));
  verificar.close();
  if (!resumo.fechamentoConfirmado) {
    sucesso = false;
    // Preserva a causa primaria da interrupcao; reporta o fechamento separadamente.
    if (normal) strcpy(resumo.motivo, "FLASH_FECHAMENTO");
    emitirErro("FLASH_FECHAMENTO", "Flash", "Gravacao sem fechamento confirmado; registro incompleto.", true);
  }
  resumo.resultado = sucesso ? "CONCLUIDO" : "FALHOU";
  concluirOperacao(idAquisicao, "ENSAIO_ENCERRADO", sucesso ? "concluido" : "falhou", resumo.motivo);
  idAquisicao = 0;
  revisaoCatalogo++;
}

void encerrarEnvio(const char *codigo, const char *mensagem) {
  arquivoEnsaio.close();
  enviandoGravado = false;
  const char *fase = !strcmp(codigo, "ENVIO_CONCLUIDO") ? "concluido" :
                     !strcmp(codigo, "ENVIO_CANCELADO") ? "cancelado" : "falhou";
  concluirOperacao(idEnvio, codigo, fase, mensagem);
  idEnvio = 0;
  idTransmissao[0] = 0;
}

// ------------------------------- ACOES ------------------------------------

bool acaoIniciar() {
  if (exclusaoTotalAtiva()) { emitirErro("CATALOGO_OCUPADO", "Flash", "Aguarde exclusao total terminar."); return false; }
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
  if (!calibracaoValida()) {
    emitirErro("CALIBRACAO_NAO_CONFIGURADA", "Calibracao", "Configure explicitamente um coeficiente em kgf antes do ensaio.");
    return false;
  }
  if (!taraPronta || taraEmAndamento) {
    emitirErro("TARA_NAO_PRONTA", "HX711", "Execute TARA sem carga e aguarde TARA_CONCLUIDA.");
    return false;
  }
  if (!taraPronta || !hx711Respondendo ||
      millis() - ultimaLeituraHX >= TIMEOUT_HX711_MS) {
    emitirErro("HX711_INDISPONIVEL", "HX711", "Confira o sensor e execute TARA sem carga.");
    return false;
  }
  if (!isfinite(payload.fatorCalibracao) || payload.fatorCalibracao == 0.0) {
    emitirErro("COEFICIENTE_INVALIDO", "ESP32", "O coeficiente deve ser finito e diferente de zero.");
    return false;
  }
  if (contadorEnsaios == UINT32_MAX) {
    emitirErro("ENSAIO_ID_ESGOTADO", "ESP32", "Reinicie para gerar uma nova sessao.");
    return false;
  }
  size_t total = LittleFS.totalBytes(), usado = LittleFS.usedBytes();
  if (usado > total || total - usado < RESERVA_FLASH + sizeof(CabecalhoEnsaio) + sizeof(RegistroAmostra) + sizeof(RodapeEnsaio)) {
    emitirErro("FLASH_SEM_ESPACO", "Flash", "Sem espaco e margem para criar outro ensaio; nenhum anterior foi removido.");
    return false;
  }
  // Um unico escritor no loop. Nunca abrir FILE_WRITE em um caminho existente.
  char novoId[32]; bool livre = false;
  for (uint8_t tentativa = 0; tentativa < 32 && contadorEnsaios < UINT32_MAX; tentativa++) {
    snprintf(novoId, sizeof(novoId), "%s-%lu", sessao, (unsigned long)++contadorEnsaios);
    caminhoEnsaio(novoId, caminhoGravacao, sizeof(caminhoGravacao));
    if (!LittleFS.exists(caminhoGravacao)) { livre = true; break; }
  }
  if (!livre) { emitirErro("ENSAIO_ID_COLISAO", "Flash", "Nao foi encontrado identificador livre; arquivos preservados."); return false; }
  arquivoEnsaio = LittleFS.open(caminhoGravacao, FILE_WRITE);
  if (!arquivoEnsaio) { emitirErro("FLASH_ABERTURA", "Flash", "Nao foi possivel criar novo arquivo."); return false; }
  revisaoCatalogo++;
  amostrasGravadas = 0;
  resumo = ResumoEnsaio();
  resumo.solicitacao = idAtual; resumo.coeficiente = payload.fatorCalibracao;
  strcpy(resumo.id, novoId);
  cabecalhoGravacao = CabecalhoEnsaio();
  CabecalhoEnsaio &cab = cabecalhoGravacao;
  cab.magic = MAGIC_ENSAIO; cab.formato = FORMATO_GRAVACAO;
  cab.cabecalhoBytes = sizeof(cab); cab.amostraBytes = sizeof(RegistroAmostra);
  strcpy(cab.ensaioId, resumo.id); cab.coeficiente = payload.fatorCalibracao;
  cab.taraInterna = taraReferencia; strcpy(cab.unidade, "kgf");
  if (pedidoAtual) {
    strcpy(cab.nome, (*pedidoAtual)["nome"] | "");
    strcpy(cab.data, (*pedidoAtual)["data"] | "");
    strcpy(cab.observacoes, (*pedidoAtual)["observacoes"] | "");
    strcpy(cab.celulaId, (*pedidoAtual)["celula_id"] | "");
    strcpy(cab.perfilId, (*pedidoAtual)["perfil_id"] | "");
  }
  cab.crc = crc32(&cab, offsetof(CabecalhoEnsaio, crc));
  bool gravou = arquivoEnsaio.write((const uint8_t *)&cab, sizeof(cab)) == sizeof(cab);
  arquivoEnsaio.flush(); ultimoFlush = millis();
  File verificar = LittleFS.open(caminhoGravacao, FILE_READ);
  CabecalhoEnsaio relido = {};
  bool confirmou = verificar && verificar.read((uint8_t *)&relido, sizeof(relido)) == sizeof(relido) &&
                  cabecalhoValido(relido) && !memcmp(&cab, &relido, sizeof(cab));
  verificar.close();
  if (!gravou || !confirmou) {
    arquivoEnsaio.close(); resumo.resultado = "FALHOU"; strcpy(resumo.motivo, "FLASH_CABECALHO");
    emitirErro("FLASH_CABECALHO", "Flash", "Novo arquivo incompleto; anteriores preservados."); return false;
  }
  resumo.resultado = "PREPARANDO";
  strcpy(resumo.motivo, "AGUARDANDO_AMOSTRA");
  ultimoIndiceLoRa = 0;
  aquisicaoIniciada = false;
  amostrasParaEnviar = 0;
  amostrasEnviadas = 0;
  inicioEnsaio = millis();
  ultimoEnvio = inicioEnsaio;
  transmitirLoRa = false;
  temLeitura = false;
  ensaioAtivo = true;
  digitalWrite(LED_EXEC, LOW);
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
  if (ensaioAtivo) {
    encerrarEnsaio("COMANDO_FINALIZAR", true);
    if (!resumo.fechamentoConfirmado) return false; // erro correlacionado ja publicado
  }
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
  if (exclusaoTotalAtiva()) { emitirErro("CATALOGO_OCUPADO", "Flash", "Aguarde exclusao total terminar."); return false; }
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
  const char *selecionado = pedidoAtual ? ((*pedidoAtual)["ensaio_id"] | resumo.id) : resumo.id;
  if (!idEnsaioValido(selecionado)) {
    emitirErro("ENSAIO_ID_OBRIGATORIO", "Flash", "Informe ensaio_id; consulte LISTAR_ENSAIOS."); return false;
  }
  if (!strcmp(selecionado, ID_LEGADO)) {
    emitirErro("FORMATO_NAO_SUPORTADO", "Flash", "Registro legado preservado; envio automatico indisponivel."); return false;
  }
  caminhoEnsaio(selecionado, caminhoTransmissao, sizeof(caminhoTransmissao));
  arquivoEnsaio = LittleFS.open(caminhoTransmissao, FILE_READ);
  if (!arquivoEnsaio || arquivoEnsaio.size() == 0) {
    if (arquivoEnsaio) arquivoEnsaio.close();
    emitirErro("SEM_ENSAIO", "Flash", "Nenhum ensaio gravado.");
    return false;
  }
  RodapeEnsaio fim = {};
  if (!lerFechamento(arquivoEnsaio, cabecalhoEnvio, fim) || fim.resultado != 1) {
    arquivoEnsaio.close();
    emitirErro("FLASH_ENSAIO_NAO_CONCLUIDO", "Flash", "Envio normal exige ensaio concluido e formato 3; arquivo incompleto, falho ou antigo.");
    return false;
  }
  if (strcmp(cabecalhoEnvio.ensaioId, selecionado) || !arquivoEnsaio.seek(sizeof(CabecalhoEnsaio))) {
    arquivoEnsaio.close();
    emitirErro("FLASH_POSICAO", "Flash", "Nao foi possivel acessar as amostras.");
    return false;
  }
  strcpy(idTransmissao, selecionado);
  amostrasParaEnviar = fim.amostras;
  amostrasEnviadas = 0;
  ultimoEnvio = millis() - INTERVALO_ENVIO_MS;
  enviandoGravado = true;
  return true;
}

bool acaoCancelarEnvio() {
  if (enviandoGravado) encerrarEnvio("ENVIO_CANCELADO", "Envio cancelado por comando.");
  return true;
}

#include "ComandosArmazenamento.h"

bool textoValido(JsonVariantConst v, size_t maximo) {
  if (!v.is<const char *>()) return false;
  JsonString s = v.as<JsonString>();
  if (s.size() > maximo || strlen(s.c_str()) != s.size()) return false;
  const uint8_t *p = (const uint8_t *)s.c_str();
  for (size_t i = 0; i < s.size();) {
    uint32_t c = p[i++];
    if (c < 128) { if (c < 32 && c != '\n' && c != '\r' && c != '\t') return false; continue; }
    uint8_t extras; uint32_t minimo;
    if (c >= 0xC2 && c <= 0xDF) { extras = 1; minimo = 0x80; c &= 0x1F; }
    else if (c >= 0xE0 && c <= 0xEF) { extras = 2; minimo = 0x800; c &= 0x0F; }
    else if (c >= 0xF0 && c <= 0xF4) { extras = 3; minimo = 0x10000; c &= 7; }
    else return false;
    if (i + extras > s.size()) return false;
    while (extras--) { uint8_t b = p[i++]; if ((b & 0xC0) != 0x80) return false; c = (c << 6) | (b & 0x3F); }
    if (c < minimo || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return false;
  }
  return true;
}

bool dataValida(const char *data) {
  if (!*data) return true;
  if (strlen(data) != 10 || data[4] != '-' || data[7] != '-') return false;
  for (uint8_t i = 0; i < 10; i++) if (i != 4 && i != 7 && !isdigit((unsigned char)data[i])) return false;
  int ano = atoi(data), mes = atoi(data + 5), dia = atoi(data + 8);
  const int dias[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (!ano || mes < 1 || mes > 12 || dia < 1) return false;
  bool bissexto = ano % 4 == 0 && (ano % 100 != 0 || ano % 400 == 0);
  return dia <= dias[mes - 1] + (mes == 2 && bissexto ? 1 : 0);
}

bool campoPermitido(const char *comando, const char *k) {
  if (!strcmp(k,"id") || !strcmp(k,"comando") || !strcmp(k,"tipo") || !strcmp(k,"protocolo") || !strcmp(k,"sessao")) return true;
  if (!strcmp(comando,"CONFIGURAR_CALIBRACAO")) return !strcmp(k,"coeficiente");
  if (!strcmp(comando,"INICIAR") || !strcmp(comando,"INICIAR_LORA"))
    return !strcmp(k,"nome") || !strcmp(k,"data") || !strcmp(k,"observacoes") || !strcmp(k,"celula_id") || !strcmp(k,"perfil_id");
  if (!strcmp(comando,"LISTAR_ENSAIOS")) return !strcmp(k,"apos") || !strcmp(k,"limite") || !strcmp(k,"revisao");
  if (!strcmp(comando,"LER_AMOSTRAS")) return !strcmp(k,"ensaio_id") || !strcmp(k,"inicio") || !strcmp(k,"limite");
  if (!strcmp(comando,"CONSULTAR_ENSAIO") || !strcmp(comando,"ENVIAR")) return !strcmp(k,"ensaio_id");
  if (!strcmp(comando,"EXCLUIR_ENSAIOS")) return !strcmp(k,"ensaio_ids");
  if (!strcmp(comando,"EXCLUIR_TODOS")) return !strcmp(k,"confirmacao");
  return false;
}

bool parametrosValidos(const JsonDocument &p) {
  const char *textos[] = {"nome","data","observacoes","celula_id","perfil_id"};
  const size_t limites[] = {48,10,128,32,32};
  for (uint8_t i = 0; i < 5; i++) if (!p[textos[i]].isNull() && !textoValido(p[textos[i]], limites[i])) return false;
  if (!p["data"].isNull() && !dataValida(p["data"].as<const char *>())) return false;
  if (!p["ensaio_id"].isNull() && (!textoValido(p["ensaio_id"],31) || !idEnsaioValido(p["ensaio_id"].as<const char *>()))) return false;
  if ((!strcmp(comandoAtual,"CONSULTAR_ENSAIO") || !strcmp(comandoAtual,"LER_AMOSTRAS")) && p["ensaio_id"].isNull()) return false;
  if (!p["limite"].isNull() && (!p["limite"].is<unsigned int>() || p["limite"].as<unsigned int>() < 1 || p["limite"].as<unsigned int>() > LIMITE_BLOCO)) return false;
  if (!p["inicio"].isNull() && (!p["inicio"].is<uint32_t>() || !p["inicio"].as<uint32_t>())) return false;
  if (!p["revisao"].isNull() && (!p["revisao"].is<uint32_t>() || !p["revisao"].as<uint32_t>())) return false;
  if (!p["apos"].isNull()) {
    if (!textoValido(p["apos"],31)) return false;
    const char *apos = p["apos"].as<const char *>();
    if (*apos && (!idEnsaioValido(apos) || p["revisao"].isNull())) return false;
  }
  if (!strcmp(comandoAtual,"EXCLUIR_ENSAIOS")) {
    if (!p["ensaio_ids"].is<JsonArrayConst>()) return false;
    JsonArrayConst ids = p["ensaio_ids"].as<JsonArrayConst>();
    if (!ids.size() || ids.size() > LIMITE_BLOCO) return false;
    for (size_t i = 0; i < ids.size(); i++) {
      if (!textoValido(ids[i],31) || !idEnsaioValido(ids[i].as<const char *>())) return false;
      for (size_t j = 0; j < i; j++) if (!strcmp(ids[i].as<const char *>(),ids[j].as<const char *>())) return false;
    }
  }
  if (!strcmp(comandoAtual,"EXCLUIR_TODOS") &&
      (!textoValido(p["confirmacao"],13) || p["confirmacao"] != "EXCLUIR_TODOS")) return false;
  return true;
}

bool normalizarParametros(const JsonDocument &pedido, char *texto, size_t n) {
  const char *chaves[] = {"nome","data","observacoes","celula_id","perfil_id","ensaio_id","ensaio_ids","apos","inicio","limite","revisao","confirmacao"};
  JsonDocument parametros;
  parametros.to<JsonObject>();
  for (const char *k : chaves) if (!pedido[k].isNull()) parametros[k] = pedido[k];
  if (parametros.overflowed() || measureJson(parametros) >= n) return false;
  serializeJson(parametros, texto, n); return true;
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

bool envelopeEstrito(const char *p, size_t &campos, double &coefLido, bool &coefNumerico) {
  campos = 0;
  pularEspacos(p);
  if (*p++ != '{') return false;
  pularEspacos(p);
  if (*p == '}') { p++; pularEspacos(p); return !*p; }
  while (*p) {
    const char *inicioChave = p;
    if (*p != '"' || !pularStringJSON(p)) return false;
    JsonDocument chave;
    if (deserializeJson(chave, inicioChave, (size_t)(p - inicioChave))) return false;
    bool chaveCoeficiente = chave.as<JsonString>() == "coeficiente";
    pularEspacos(p);
    if (*p++ != ':') return false;
    pularEspacos(p);
    if (*p == '"') { if (!pularStringJSON(p)) return false; }
    else if (*p == '[') {
      p++; pularEspacos(p);
      if (*p != ']') while (true) {
        if (*p != '"' || !pularStringJSON(p)) return false;
        pularEspacos(p);
        if (*p == ']') break;
        if (*p++ != ',') return false;
        pularEspacos(p);
      }
      if (*p++ != ']') return false;
    } else {
      const char *inicioNumero = p;
      // Numero JSON estrito; tipos de id/protocolo continuam validados abaixo.
      if (*p == '-') p++;
      if (!isdigit((unsigned char)*p)) return false;
      if (*p == '0') p++;
      else while (isdigit((unsigned char)*p)) p++;
      if (*p == '.') {
        p++;
        if (!isdigit((unsigned char)*p)) return false;
        while (isdigit((unsigned char)*p)) p++;
      }
      if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!isdigit((unsigned char)*p)) return false;
        while (isdigit((unsigned char)*p)) p++;
      }
      if (chaveCoeficiente) {
        // Leitura direta em double evita a compactacao intermediaria em float
        // que algumas versoes do ArduinoJson aplicam a numeros decimais curtos.
        char *fim = nullptr;
        coefLido = strtod(inicioNumero, &fim);
        if (fim != p) return false;
        coefNumerico = true;
      }
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
  double coefLido = 0;
  bool coefNumerico = false;
  JsonDocument pedido;
  if (!envelopeEstrito(linha, campos, coefLido, coefNumerico)) {
    emitirErro("JSON_INVALIDO", "USB", "Esperado um objeto JSON plano, completo e sem texto adicional.");
    return;
  }
  DeserializationError erro = deserializeJson(pedido, linha, DeserializationOption::NestingLimit(3));
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
    ~LimparContexto() { idAtual = 0; comandoAtual = nullptr; registroAtual = -1; pedidoAtual = nullptr; }
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
    if (strlen(k) != campo.key().size() || !campoPermitido(comandoAtual, k)) {
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
  bool configurar = !strcmp(comandoAtual, "CONFIGURAR_CALIBRACAO");
  double coeficiente = 0;
  bool possuiCoeficiente = !pedido["coeficiente"].isNull();
  if (configurar) {
    if (!coefNumerico || !isfinite(coefLido) || coefLido == 0.0) {
      emitirErro("COEFICIENTE_INVALIDO", "Calibracao", "coeficiente deve ser numero JSON finito e nao zero, em contagens/kgf."); return;
    }
    coeficiente = coefLido;
  } else if (possuiCoeficiente) {
    emitirErro("CAMPO_DESCONHECIDO", "USB", "coeficiente somente e aceito em CONFIGURAR_CALIBRACAO."); return;
  }
  if (!parametrosValidos(pedido)) {
    emitirErro("PARAMETRO_INVALIDO", "USB", "Confira campos, limites, data, identificadores e confirmacao; nenhuma acao executada."); return;
  }
  char parametros[513];
  if (!normalizarParametros(pedido, parametros, sizeof(parametros))) {
    emitirErro("PARAMETRO_INVALIDO", "USB", "Parametros excedem o limite interno."); return;
  }
  pedidoAtual = &pedido;
  int anterior = buscarRegistro(idAtual);
  if (anterior >= 0) {
    if (strcmp(registros[anterior].comando, comandoAtual) || (configurar && registros[anterior].coeficiente != coeficiente) || strcmp(registros[anterior].parametros, parametros)) {
      emitirErro("ID_CONFLITO", "USB", "Este id ja pertence a outro comando ou coeficiente."); return;
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
  registros[escolhido].pendente = false;
  registros[escolhido].resposta[0] = registros[escolhido].conclusao[0] = 0;
  registros[escolhido].id = maiorId = idAtual;
  strcpy(registros[escolhido].comando, comandoAtual);
  registros[escolhido].coeficiente = coeficiente;
  strcpy(registros[escolhido].parametros, parametros);
  bool ok = false, demorada = false;
  if (!strcmp(comandoAtual, "IDENTIFICAR")) { informarIdentidade(); return; }
  if (!strcmp(comandoAtual, "STATUS")) { informarEstado(); return; }
  if (!strcmp(comandoAtual, "AJUDA")) { informarAjuda(); return; }
  if (!strcmp(comandoAtual, "CONSULTAR_CALIBRACAO")) { consultarCalibracao(); return; }
  if (!strcmp(comandoAtual,"ESPACO")) { consultarEspaco(); return; }
  if (!strcmp(comandoAtual,"CONSULTAR_ENSAIO")) { consultarDetalhes(pedido["ensaio_id"]); return; }
  if (!strcmp(comandoAtual,"LER_AMOSTRAS")) { recuperarBloco(pedido["ensaio_id"], pedido["inicio"] | 1U, pedido["limite"] | LIMITE_BLOCO); return; }
  if (!strcmp(comandoAtual,"EXCLUIR_ENSAIOS")) { excluirSelecionados(pedido["ensaio_ids"].as<JsonArrayConst>()); return; }
  if (!strcmp(comandoAtual,"LISTAR_ENSAIOS") || !strcmp(comandoAtual,"EXCLUIR_TODOS")) {
    bool excluir = !strcmp(comandoAtual,"EXCLUIR_TODOS");
    if (iniciarCatalogo(excluir, pedido["apos"] | "", pedido["limite"] | LIMITE_BLOCO, pedido["revisao"] | 0U)) {
      registros[registroAtual].pendente = true; responderOK(true);
    }
    return;
  }
  if (configurar) ok = acaoConfigurarCalibracao(coeficiente);
  else if (!strcmp(comandoAtual, "TARA")) { ok = acaoTara(); demorada = true; }
  else if (!strcmp(comandoAtual, "INICIAR")) { ok = opcoes.iniciar(); demorada = true; }
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
      if (!strcmp(comandoAtual, "TARA")) idTara = idAtual;
      else if (enviandoGravado) idEnvio = idAtual;
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
  emitirEvento("BOOT", "Bancada V0.16 iniciando.");

  memoriaPronta = LittleFS.begin(false); // Nunca formatar automaticamente na inicializacao
  if (!memoriaPronta) emitirErro("FLASH_INICIALIZACAO", "Flash", "Falha ao montar a flash interna.");
  if (memoriaPronta && !LittleFS.exists(DIRETORIO_ENSAIOS) && !LittleFS.mkdir(DIRETORIO_ENSAIOS)) {
    memoriaPronta = false;
    emitirErro("FLASH_DIRETORIO", "Flash", "Nao foi possivel preparar o diretorio; conteudo preservado.");
  }

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
  iniciarTara(0);
  if (FATOR_CALIBRACAO_PADRAO == 1.0) {
    JsonDocument doc;
    prepararMensagem(doc, "aviso");
    doc["nivel"] = "AVISO";
    doc["origem"] = "ESP32";
    doc["codigo"] = "COEFICIENTE_PADRAO";
    doc["descricao"] = "Valor inicial 1.0 nao configurado; ensaio bloqueado ate CONFIGURAR_CALIBRACAO e TARA.";
    publicar(doc);
  }
  emitirEvento("USB_PRONTA", "Use IDENTIFICAR com id 0; tara inicial em andamento.");
}

void loop() {
  atenderSaidaUSB();
  ReceberComando();
  atualizarTara();
  if (loraTransmitindo && millis() - inicioPacoteLoRa >= 5000) {
    LoRa.idle();
    loraTransmitindo = false;
    loraPronto = transmitirLoRa = false;
    emitirErro("LORA_TX_TIMEOUT", "LoRa", "Sem TX_DONE em 5000 ms; radio desabilitado ate reiniciar.");
    if (enviandoGravado) encerrarEnvio("ENVIO_INTERROMPIDO", "Sem confirmacao local do fim de TX.");
    else emitirEvento("ESTADO_ATUALIZADO", "Envio ao vivo desabilitado por timeout do radio.");
  }
  digitalWrite(LED_EXEC, ensaioAtivo && aquisicaoIniciada);
  // O amarelo acompanha a atividade de TX local, inclusive o ultimo pacote.
  digitalWrite(LED_LORA, loraPronto && loraTransmitindo);

  if (!taraEmAndamento && celula.is_ready()) {
    // get_value ja subtrai OFFSET. Nao usar get_units ou subtrair novamente.
    long leitura = (long)celula.get_value();
    ultimaLeituraHX = millis();
    hx711Respondendo = true;
    if (ensaioAtivo) {
      if (!aquisicaoIniciada) inicioEnsaio = millis();
      payload.tempo_ms = (uint32_t)(millis() - inicioEnsaio);
      payload.indice = amostrasGravadas + 1;
      strcpy(payload.ensaioId, resumo.id);
      payload.empuxo = (double)leitura / payload.fatorCalibracao;
      RegistroAmostra registro = {};
      registro.dados = payload;
      registro.crc = crc32(&registro, offsetof(RegistroAmostra, crc));
      size_t total = LittleFS.totalBytes(), usado = LittleFS.usedBytes();
      if (!isfinite(payload.empuxo)) {
        emitirErro("MEDICAO_INVALIDA", "HX711", "O calculo de empuxo produziu um valor nao finito.");
        encerrarEnsaio("MEDICAO_INVALIDA");
      } else if (amostrasGravadas == UINT32_MAX) {
        emitirErro("LIMITE_AMOSTRAS", "ESP32", "Limite do contador de amostras atingido.");
        encerrarEnsaio("LIMITE_AMOSTRAS");
      } else if (usado > total || total - usado < RESERVA_FLASH + sizeof(RegistroAmostra) + sizeof(RodapeEnsaio)) {
        emitirErro("FLASH_SEM_ESPACO", "Flash", "Sem espaco para outra amostra e encerramento.");
        encerrarEnsaio("FLASH_SEM_ESPACO");
      } else if (arquivoEnsaio.write((const uint8_t *)&registro, sizeof(registro)) == sizeof(registro)) {
        temLeitura = true;
        resumo.amostras = ++amostrasGravadas;
        if (!aquisicaoIniciada) {
          aquisicaoIniciada = resumo.iniciado = true;
          resumo.resultado = "ADQUIRINDO";
          strcpy(resumo.motivo, "EM_ANDAMENTO");
          digitalWrite(LED_EXEC, HIGH);
          JsonDocument doc;
          prepararMensagem(doc, "evento");
          doc["id"] = idAquisicao;
          doc["codigo"] = "AQUISICAO_INICIADA";
          doc["fase"] = "iniciado";
          doc["estado"] = estadoAtual();
          doc["descricao"] = "Primeira amostra calibrada gravada; aquisicao em andamento.";
          adicionarEnsaio(doc);
          publicar(doc);
        }
        informarAmostra();
        if (millis() - ultimoFlush >= 1000) { arquivoEnsaio.flush(); ultimoFlush = millis(); }
      } else {
        emitirErro("FLASH_GRAVACAO", "Flash", "Falha ao gravar uma amostra; registro nao sera concluido normalmente.");
        encerrarEnsaio("FLASH_GRAVACAO");
      }
    }
  } else if (!taraEmAndamento && hx711Respondendo && millis() - ultimaLeituraHX >= TIMEOUT_HX711_MS) {
    hx711Respondendo = false;
    taraPronta = false;
    emitirErro("HX711_TIMEOUT", "HX711", "Sem novas leituras ha 2000 ms. Confira o sensor e execute TARA sem carga.");
    if (ensaioAtivo) encerrarEnsaio("HX711_TIMEOUT");
  }

  if (ensaioAtivo && loraPronto && transmitirLoRa && temLeitura && payload.indice != ultimoIndiceLoRa) {
    unsigned long agora = millis();
    if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
      int resultado = transmitirPayload(payload);
      if (resultado > 0) { ultimoIndiceLoRa = payload.indice; atualizarRitmoEnvio(agora); }
      else if (resultado < 0) transmitirLoRa = false;
    }
  }

  if (enviandoGravado) {
    unsigned long agora = millis();
    if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
      if (amostrasEnviadas >= amostrasParaEnviar) {
        if (!loraTransmitindo) encerrarEnvio("ENVIO_CONCLUIDO", "Todos os pacotes foram transmitidos localmente; recepcao nao confirmada.");
      } else {
        Payload gravado;
        if (!lerAmostra(arquivoEnsaio, cabecalhoEnvio, amostrasEnviadas + 1, gravado)) {
          emitirErro("FLASH_AMOSTRA_INVALIDA", "Flash", "Falha de leitura, CRC ou identidade da amostra gravada.");
          encerrarEnvio("ENVIO_INTERROMPIDO", "Interrompido por falha de leitura.");
        } else {
          int resultado = transmitirPayload(gravado);
          if (resultado > 0) {
            amostrasEnviadas++;
            atualizarRitmoEnvio(agora);
          } else if (resultado < 0) {
            encerrarEnvio("ENVIO_INTERROMPIDO", "Interrompido por falha no envio LoRa.");
          }
        }
      }
    }
  }
  atualizarCatalogo();
}
