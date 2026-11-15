// Bancada de Empuxo V0.12 - Etapa 1: protocolo serial e diagnosticos.
// Base: Bancada_Empuxo_V0.9(2).ino. Comandos de texto terminados por Enter.
// Todas as mensagens da aplicacao na serial sao JSON, uma por linha.
#include <Arduino.h>
#include <HX711.h>
#include <SPI.h>
#include <LoRa.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // ArduinoJson 7.x
#include <ctype.h>
#include <math.h>
#include <string.h>

const char *VERSAO_FIRMWARE = "0.12";
const int VERSAO_PROTOCOLO = 1;

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
const size_t TAMANHO_COMANDO = 64; // ate 63 caracteres, sem CR/LF

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

char bufferComando[TAMANHO_COMANDO];
size_t tamanhoComando = 0;
bool descartarLinha = false;
const char *comandoAtual = nullptr; // valido somente durante o processamento

// --------------------------- PROTOCOLO SERIAL ------------------------------

const char *estadoAtual() {
  if (ensaioAtivo) return "ADQUIRINDO";
  if (enviandoGravado) return "ENVIANDO_GRAVADO";
  return "OCIOSO";
}

void prepararMensagem(JsonDocument &doc, const char *tipo) {
  doc["protocolo"] = VERSAO_PROTOCOLO;
  doc["tipo"] = tipo;
  doc["ms"] = millis(); // tempo desde o boot, nao data/hora de calendario
  if (comandoAtual) doc["comando"] = comandoAtual;
}

void publicar(JsonDocument &doc) {
  if (doc.overflowed()) {
    // Reserva fixa: nao tenta alocar outro JsonDocument em uma falha de RAM.
    Serial.println("{\"protocolo\":1,\"tipo\":\"erro\",\"ok\":false,\"nivel\":\"ERRO\",\"origem\":\"ESP32\",\"codigo\":\"JSON_MEMORIA\",\"mensagem\":\"Sem RAM para montar a resposta.\"}");
    return;
  }
  serializeJson(doc, Serial);
  Serial.println();
}

void emitirErro(const char *codigo, const char *origem, const char *mensagem) {
  JsonDocument doc;
  prepararMensagem(doc, "erro");
  doc["ok"] = false;
  doc["nivel"] = "ERRO";
  doc["origem"] = origem;
  doc["codigo"] = codigo;
  doc["mensagem"] = mensagem;
  publicar(doc);
}

void emitirEvento(const char *codigo, const char *mensagem) {
  JsonDocument doc;
  prepararMensagem(doc, "evento");
  doc["codigo"] = codigo;
  doc["mensagem"] = mensagem;
  doc["estado"] = estadoAtual();
  doc["amostras_gravadas"] = amostrasGravadas;
  doc["amostras_enviadas"] = amostrasEnviadas;
  publicar(doc);
}

void responderOK() {
  JsonDocument doc;
  prepararMensagem(doc, "resposta");
  doc["ok"] = true;
  doc["estado"] = estadoAtual();
  doc["lora_ao_vivo"] = transmitirLoRa;
  doc["amostras_gravadas"] = amostrasGravadas;
  doc["amostras_enviadas"] = amostrasEnviadas;
  publicar(doc);
}

void informarEstado() {
  JsonDocument doc;
  prepararMensagem(doc, "status");
  doc["ok"] = true;
  doc["firmware"] = VERSAO_FIRMWARE;
  doc["estado"] = estadoAtual();
  doc["unidade"] = "kgf";
  doc["fator_calibracao"] = payload.fatorCalibracao;
  doc["origem_coeficiente"] = "firmware";
  doc["tara_pronta"] = taraPronta;
  doc["hx711_respondendo"] = hx711Respondendo &&
      (millis() - ultimaLeituraHX < TIMEOUT_HX711_MS);
  doc["memoria_pronta"] = memoriaPronta;
  doc["lora_pronto"] = loraPronto; // inicializado, nao confirma receptor
  doc["lora_ao_vivo"] = transmitirLoRa;
  doc["enviando_gravado"] = enviandoGravado;
  doc["motor_implementado"] = false;
  doc["amostras_gravadas"] = amostrasGravadas;
  doc["amostras_para_enviar"] = amostrasParaEnviar;
  doc["amostras_enviadas"] = amostrasEnviadas;
  publicar(doc);
}

void informarAjuda() {
  JsonDocument doc;
  prepararMensagem(doc, "ajuda");
  doc["ok"] = true;
  doc["firmware"] = VERSAO_FIRMWARE;
  doc["comandos"] = "STATUS/s; AJUDA/?; INICIAR/i; INICIAR_LORA/a; FINALIZAR/f; LORA_LIGAR; LORA_DESLIGAR; LORA_ALTERNAR/l; ENVIAR/e; CANCELAR/c";
  doc["formato"] = "Um comando de texto por linha, terminada em LF ou CR. Ate 63 caracteres.";
  publicar(doc);
}

void informarAmostra() {
  JsonDocument doc;
  // Mensagem curta para caber em 115200 baud mesmo com HX711 a 80 SPS.
  // Unidade e coeficiente fixos no ensaio podem ser consultados em STATUS.
  doc["protocolo"] = VERSAO_PROTOCOLO;
  doc["tipo"] = "amostra";
  doc["tempo_ms"] = payload.tempo_ms;
  doc["empuxo_kgf"] = payload.empuxo;
  doc["indice"] = amostrasGravadas; // 1 para a primeira amostra gravada
  publicar(doc);
}

// ---------------------------- HX711 E LORA ---------------------------------

bool executarTaraInicial() {
  // Mesmas 10 leituras da tara original, mas com um limite total de espera.
  // Nao cria um comando de offset ou de tara; a tara ainda ocorre no boot.
  int64_t soma = 0;
  uint8_t quantidade = 0;
  unsigned long inicio = millis();
  while (quantidade < 10 && millis() - inicio < TIMEOUT_TARA_MS) {
    if (celula.is_ready()) {
      soma += celula.read();
      quantidade++;
    }
    delay(1);
  }
  if (quantidade != 10) return false;
  celula.set_offset((long)(soma / 10));
  ultimaLeituraHX = millis();
  hx711Respondendo = true;
  return true;
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
  emitirEvento("ENSAIO_ENCERRADO", motivo);
}

void encerrarEnvio(const char *codigo, const char *mensagem) {
  arquivoEnsaio.close();
  enviandoGravado = false;
  emitirEvento(codigo, mensagem);
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

// -------------------------- RECEPCAO DE COMANDOS ---------------------------

void processarComando(char *linha) {
  while (*linha == ' ' || *linha == '\t') linha++;
  size_t tamanho = strlen(linha);
  while (tamanho && (linha[tamanho - 1] == ' ' || linha[tamanho - 1] == '\t')) {
    linha[--tamanho] = '\0';
  }
  if (!tamanho) return;
  for (size_t i = 0; i < tamanho; i++) linha[i] = toupper((unsigned char)linha[i]);
  comandoAtual = linha;
  bool ok = false;
  bool responder = true;

  if (!strcmp(linha, "STATUS") || !strcmp(linha, "S")) {
    informarEstado();
    responder = false;
  } else if (!strcmp(linha, "AJUDA") || !strcmp(linha, "?")) {
    informarAjuda();
    responder = false;
  } else if (!strcmp(linha, "INICIAR") || !strcmp(linha, "I")) {
    ok = opcoes.iniciar();
  } else if (!strcmp(linha, "INICIAR_LORA") || !strcmp(linha, "A")) {
    ok = opcoes.iniciarComLoRa();
  } else if (!strcmp(linha, "FINALIZAR") || !strcmp(linha, "F")) {
    ok = opcoes.finalizar();
  } else if (!strcmp(linha, "LORA_LIGAR")) {
    ok = opcoes.ligarLoRa();
  } else if (!strcmp(linha, "LORA_DESLIGAR")) {
    ok = opcoes.desligarLoRa();
  } else if (!strcmp(linha, "LORA_ALTERNAR") || !strcmp(linha, "L")) {
    ok = transmitirLoRa ? opcoes.desligarLoRa() : opcoes.ligarLoRa();
  } else if (!strcmp(linha, "ENVIAR") || !strcmp(linha, "E")) {
    ok = opcoes.enviarGravado();
  } else if (!strcmp(linha, "CANCELAR") || !strcmp(linha, "C")) {
    ok = opcoes.cancelarEnvio();
  } else {
    emitirErro("COMANDO_DESCONHECIDO", "Serial", "Comando nao reconhecido. Use AJUDA.");
  }
  if (responder && ok) responderOK();
  comandoAtual = nullptr;
}

void ReceberComando() {
  // Limita o trabalho de recepcao por loop para preservar a aquisicao.
  for (uint8_t i = 0; i < 32 && Serial.available(); i++) {
    char recebido = (char)Serial.read();
    if (recebido == '\n' || recebido == '\r') {
      if (!descartarLinha && tamanhoComando) {
        bufferComando[tamanhoComando] = '\0';
        processarComando(bufferComando);
      }
      tamanhoComando = 0;
      descartarLinha = false;
      return;
    }
    if (descartarLinha) continue;
    if ((unsigned char)recebido < 32 && recebido != '\t') {
      emitirErro("COMANDO_CARACTERE", "Serial", "Caractere de controle invalido; linha descartada.");
      descartarLinha = true;
    } else if ((unsigned char)recebido > 126) {
      emitirErro("COMANDO_CARACTERE", "Serial", "Use somente caracteres ASCII nos comandos.");
      descartarLinha = true;
    } else if (tamanhoComando >= TAMANHO_COMANDO - 1) {
      emitirErro("COMANDO_LONGO", "Serial", "Limite de 63 caracteres excedido; linha descartada.");
      descartarLinha = true;
    } else {
      bufferComando[tamanhoComando++] = recebido;
    }
  }
}

// ----------------------------- SETUP / LOOP -------------------------------

void setup() {
  Serial.begin(115200);
  pinMode(LED_EXEC, OUTPUT);
  pinMode(LED_LORA, OUTPUT);
  digitalWrite(LED_EXEC, LOW);
  digitalWrite(LED_LORA, LOW);
  emitirEvento("BOOT", "Bancada V0.12 iniciando.");

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
  taraPronta = executarTaraInicial();
  if (!taraPronta) emitirErro("HX711_TARA_TIMEOUT", "HX711", "Tara inicial incompleta em 3000 ms. Confira o sensor e reinicie.");
  if (FATOR_CALIBRACAO_PADRAO == 1.0) {
    JsonDocument doc;
    prepararMensagem(doc, "aviso");
    doc["nivel"] = "AVISO";
    doc["origem"] = "ESP32";
    doc["codigo"] = "COEFICIENTE_PADRAO";
    doc["mensagem"] = "Coeficiente 1.0 preservado da V0.9. Valores nao representam kgf validados ate calibrar.";
    publicar(doc);
  }
  emitirEvento("PRONTO", "Inicializacao concluida. Consulte STATUS e AJUDA; PRONTO nao garante sensores disponiveis.");
  informarEstado();
}

void loop() {
  ReceberComando();
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
