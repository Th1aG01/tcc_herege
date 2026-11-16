#pragma once
// Transferencia completa: stop-and-wait, polling RX fora da ISR, arquivo somente leitura.
// Wire BT/1: inteiros little-endian, IDs ASCII de 32 bytes, CRC32 por quadro.
const size_t LORA_CAB_BYTES = 86, LORA_DADOS_BYTES = 128, LORA_QUADRO_MAX = 218;
const uint32_t ESPERA_ACK_MS = 2000, LIMITE_TRANSFERENCIA_MS = 7200000;
const uint8_t MAX_TENTATIVAS = 4, MAX_REPETICOES = 64;
enum TipoQuadro : uint8_t { BT_INICIO=1, BT_DADOS=2, BT_FIM=3, BT_ACK=4, BT_PEDIR=5, BT_CONFIRMAR=6 };
struct Transferencia {
  enum Etapa { NENHUMA, VALIDANDO, HASH_ARQUIVO, PRONTA, TX, ESPERANDO, TERMINADA } etapa = NENHUMA;
  char id[32] = {}, ensaio[32] = {};
  uint32_t solicitacao=0, totalBytes=0, crcArquivo=0, crcParcial=0xffffffffU, hashPos=0;
  uint32_t amostras=0, validarIndice=1, blocos=0, seq=0, confirmados=0;
  uint32_t quadrosTX=0, ignorados=0, tentativas=0, repeticoes=0;
  unsigned long inicio=0, espera=0, ultimoTX=0, progresso=0;
  bool reparo=false, localConcluido=false, confirmado=false, iniciado=false;
  const char *resultado="NENHUMA";
  CabecalhoEnsaio cab = {};
};
Transferencia transferencia;
uint32_t contadorTransferencias=0;

void put32(uint8_t *p,uint32_t v) { for(uint8_t i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i)); }
uint32_t get32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
const char *etapaTransferencia() {
  switch(transferencia.etapa) {
    case Transferencia::VALIDANDO: return "VALIDANDO";
    case Transferencia::HASH_ARQUIVO: return "PREPARANDO";
    case Transferencia::PRONTA: return "AGENDADA";
    case Transferencia::TX: return "TRANSMITINDO";
    case Transferencia::ESPERANDO: return transferencia.seq==transferencia.blocos+1 ? "AGUARDANDO_CONFIRMACAO_FINAL":"AGUARDANDO_ACK";
    case Transferencia::TERMINADA: return transferencia.resultado;
    default: return "NENHUMA";
  }
}
uint32_t amostrasConfirmadasEmBlocos() {
  uint64_t bytes=(uint64_t)transferencia.confirmados*LORA_DADOS_BYTES;
  if(bytes>transferencia.totalBytes)bytes=transferencia.totalBytes;
  if(bytes<=sizeof(CabecalhoEnsaio))return 0;
  uint32_t n=(bytes-sizeof(CabecalhoEnsaio))/sizeof(RegistroAmostra);
  return n>transferencia.amostras?transferencia.amostras:n;
}
void adicionarTransferencia(JsonDocument &doc) {
  auto t=doc["transferencia"].to<JsonObject>();
  t["id"]=transferencia.id; t["ensaio_id"]=transferencia.ensaio;
  t["solicitacao_id"]=transferencia.solicitacao; t["ativa"]=enviandoGravado;
  t["estado"]=etapaTransferencia(); t["amostras_total"]=transferencia.amostras;
  t["amostras_em_blocos_ack"]=amostrasConfirmadasEmBlocos();
  t["blocos_total"]=transferencia.blocos; t["blocos_ack"]=transferencia.confirmados;
  t["sequencia_atual"]=transferencia.seq; t["quadros_tx"]=transferencia.quadrosTX;
  t["tentativas_atual"]=transferencia.tentativas; t["retransmissoes"]=transferencia.repeticoes;
  t["controles_ignorados"]=transferencia.ignorados; t["bytes_arquivo"]=transferencia.totalBytes;
  t["crc32_arquivo"]=transferencia.crcArquivo;
  t["envio_local_concluido"]=transferencia.localConcluido;
  t["recebimento_confirmado"]=transferencia.confirmado;
}
void estadoTransferenciaResumido(JsonDocument &doc) {
  doc["radio"]["transferencia_id"]=transferencia.id;
  doc["radio"]["estado_transferencia"]=etapaTransferencia();
  doc["radio"]["recebimento_confirmado"]=transferencia.confirmado;
}
void eventoTransferencia(const char *codigo,const char *fase,const char *descricao,bool terminal=false) {
  JsonDocument doc;prepararMensagem(doc,"evento");doc.remove("comando");
  doc["id"]=transferencia.solicitacao;
  if(idAtual)doc["causado_por"]=idAtual;
  doc["codigo"]=codigo;doc["fase"]=fase;doc["origem"]=!strncmp(codigo,"FLASH_",6)?"Flash":"LoRa";doc["descricao"]=descricao;
  adicionarTransferencia(doc);
  if(terminal) {
    int i=buscarRegistro(transferencia.solicitacao);
    if(i>=0) {
      registros[i].pendente=false;
      if(!doc.overflowed()&&measureJson(doc)<RESPOSTA_MAX)
        serializeJson(doc,registros[i].conclusao,sizeof(registros[i].conclusao));
    }
  }
  publicar(doc);
}
void terminarTransferencia(const char *codigo,const char *fase,const char *descricao) {
  // Cancelamento local aborta RX/TX; nao equivale a cancelamento confirmado remotamente.
  LoRa.idle();loraTransmitindo=false;digitalWrite(LED_LORA,LOW);
  LoRa.onTxDone(nullptr);
  arquivoEnsaio.close();enviandoGravado=false;idEnvio=0;idTransmissao[0]=0;
  transferencia.etapa=Transferencia::TERMINADA;transferencia.resultado=codigo;
  eventoTransferencia(codigo,fase,descricao,true);
}
bool selecionarTransferencia() {
  if(contadorTransferencias==UINT32_MAX) { emitirErro("TRANSFERENCIA_ID_ESGOTADO","LoRa","Reinicie apos concluir as operacoes.");return false; }
  transferencia=Transferencia();
  snprintf(transferencia.id,sizeof(transferencia.id),"%s-T%lu",sessao,(unsigned long)++contadorTransferencias);
  strcpy(transferencia.ensaio,idTransmissao);transferencia.solicitacao=idAtual;
  transferencia.cab=cabecalhoEnvio;transferencia.amostras=amostrasParaEnviar;
  transferencia.totalBytes=arquivoEnsaio.size();
  transferencia.blocos=(transferencia.totalBytes+LORA_DADOS_BYTES-1)/LORA_DADOS_BYTES;
  transferencia.inicio=transferencia.progresso=millis();
  transferencia.ultimoTX=millis()-INTERVALO_ENVIO_MS;
  transferencia.etapa=Transferencia::VALIDANDO;transferencia.resultado="EM_ANDAMENTO";
  return true;
}
void responderTransferencia(bool aceita) {
  JsonDocument doc;prepararMensagem(doc,"resposta");doc["ok"]=true;doc["fase"]=aceita?"aceito":"concluido";
  adicionarTransferencia(doc);publicar(doc);
}
bool conferirIdTransferencia() {
  if(pedidoAtual && !(*pedidoAtual)["transferencia_id"].isNull() &&
     strcmp((*pedidoAtual)["transferencia_id"].as<const char*>(),transferencia.id)) {
    emitirErro("TRANSFERENCIA_NAO_ENCONTRADA","LoRa","Esta nao e a transferencia atual ou ultima retida.");return false;
  }
  return true;
}
size_t montarQuadro(uint8_t *buf,uint8_t tipo,uint32_t seq,const uint8_t *dados,size_t n) {
  if(n>LORA_DADOS_BYTES)return 0;
  memset(buf,0,LORA_QUADRO_MAX);buf[0]='B';buf[1]='T';buf[2]=1;buf[3]=tipo;
  memcpy(buf+4,transferencia.id,strlen(transferencia.id));
  memcpy(buf+36,transferencia.ensaio,strlen(transferencia.ensaio));
  put32(buf+68,seq);put32(buf+72,transferencia.blocos);put32(buf+76,transferencia.totalBytes);put32(buf+80,transferencia.crcArquivo);
  buf[84]=(uint8_t)n;buf[85]=(uint8_t)(n>>8);
  if(n)memcpy(buf+86,dados,n);
  put32(buf+86+n,crc32(buf,86+n));return 90+n;
}
void repetirQuadro() {
  if(transferencia.tentativas>=MAX_TENTATIVAS||transferencia.repeticoes>=MAX_REPETICOES) {
    terminarTransferencia("ENVIO_SEM_CONFIRMACAO","falhou","Limite de espera/tentativas atingido; recebimento integral nao confirmado.");return;
  }
  transferencia.repeticoes++;transferencia.etapa=Transferencia::PRONTA;
}
void receberControleTransferencia() {
  // RX e TX usam polling: a biblioteca nao instala ISR com acesso SPI.
  int n=LoRa.parsePacket();if(n<=0)return;
  uint8_t buf[LORA_QUADRO_MAX];size_t lidos=0;
  while(LoRa.available()&&lidos<255) {
    int b=LoRa.read();if(lidos<LORA_QUADRO_MAX)buf[lidos]=(uint8_t)b;lidos++;
  }
  if(n!=90||lidos!=90||buf[0]!='B'||buf[1]!='T'||buf[2]!=1||buf[84]||buf[85]||
     get32(buf+86)!=crc32(buf,86)||memcmp(buf+4,transferencia.id,32)||memcmp(buf+36,transferencia.ensaio,32)||
     get32(buf+72)!=transferencia.blocos||get32(buf+76)!=transferencia.totalBytes||get32(buf+80)!=transferencia.crcArquivo) {
    transferencia.ignorados++;return;
  }
  uint32_t seq=get32(buf+68);uint8_t tipo=buf[3];
  if(tipo==BT_CONFIRMAR && transferencia.seq==transferencia.blocos+1 && seq==transferencia.seq && transferencia.localConcluido && !transferencia.reparo) {
    transferencia.confirmado=true;
    terminarTransferencia("ENVIO_CONFIRMADO","concluido","Receptor confirmou arquivo completo e CRC32; original preservado.");return;
  }
  if(tipo==BT_PEDIR) {
    if(seq==transferencia.seq) { repetirQuadro();return; }
    if(transferencia.seq==transferencia.blocos+1 && seq<=transferencia.blocos && transferencia.localConcluido &&
       transferencia.repeticoes<MAX_REPETICOES) {
      transferencia.seq=seq;transferencia.reparo=true;transferencia.tentativas=0;
      transferencia.repeticoes++;transferencia.etapa=Transferencia::PRONTA;return;
    }
  }
  if(tipo==BT_ACK && seq==transferencia.seq && seq<=transferencia.blocos) {
    if(transferencia.reparo) { transferencia.seq=transferencia.blocos+1;transferencia.reparo=false; }
    else { if(seq)transferencia.confirmados=seq;transferencia.seq++; }
    transferencia.tentativas=0;transferencia.etapa=Transferencia::PRONTA;
    amostrasEnviadas=amostrasConfirmadasEmBlocos();
    if(millis()-transferencia.progresso>=1000) {
      transferencia.progresso=millis();eventoTransferencia("TRANSFERENCIA_PROGRESSO","andamento","Blocos confirmados; aguarde confirmacao integral.");
    }
    return;
  }
  // ACK do FIM, ACK antigo, controles futuros e mensagens de outra fase nao reiniciam o prazo.
  transferencia.ignorados++;
}
void atualizarTransferencia() {
  if(!enviandoGravado)return;
  if(millis()-transferencia.inicio>=LIMITE_TRANSFERENCIA_MS) {
    terminarTransferencia("ENVIO_PRAZO_TOTAL","falhou","Prazo total de duas horas excedido; arquivo preservado.");return;
  }
  if(transferencia.etapa==Transferencia::VALIDANDO) {
    if(transferencia.validarIndice<=transferencia.amostras) {
      Payload p;
      if(!lerAmostra(arquivoEnsaio,transferencia.cab,transferencia.validarIndice++,p))
        terminarTransferencia("FLASH_AMOSTRA_INVALIDA","falhou","Falha de leitura/CRC na verificacao do arquivo.");
      return;
    }
    transferencia.etapa=Transferencia::HASH_ARQUIVO;return;
  }
  if(transferencia.etapa==Transferencia::HASH_ARQUIVO) {
    uint8_t dados[LORA_DADOS_BYTES];size_t n=transferencia.totalBytes-transferencia.hashPos;if(n>LORA_DADOS_BYTES)n=LORA_DADOS_BYTES;
    if(n) {
      if(!arquivoEnsaio.seek(transferencia.hashPos)||arquivoEnsaio.read(dados,n)!=n) {
        terminarTransferencia("FLASH_LEITURA","falhou","Falha ao preparar CRC do arquivo.");return;
      }
      for(size_t i=0;i<n;i++) {
        transferencia.crcParcial^=dados[i];
        for(uint8_t bit=0;bit<8;bit++)transferencia.crcParcial=(transferencia.crcParcial>>1)^(0xedb88320U&(0U-(transferencia.crcParcial&1U)));
      }
      transferencia.hashPos+=n;return;
    }
    transferencia.crcArquivo=~transferencia.crcParcial;transferencia.etapa=Transferencia::PRONTA;return;
  }
  if(transferencia.etapa==Transferencia::PRONTA) {
    if(loraTransmitindo||millis()-transferencia.ultimoTX<INTERVALO_ENVIO_MS)return;
    uint8_t dados[LORA_DADOS_BYTES],quadro[LORA_QUADRO_MAX];size_t n=0;
    uint8_t tipo=transferencia.seq==0?BT_INICIO:transferencia.seq==transferencia.blocos+1?BT_FIM:BT_DADOS;
    if(tipo==BT_DADOS) {
      uint32_t offset=(transferencia.seq-1)*LORA_DADOS_BYTES;n=transferencia.totalBytes-offset;if(n>LORA_DADOS_BYTES)n=LORA_DADOS_BYTES;
      if(!arquivoEnsaio.seek(offset)||arquivoEnsaio.read(dados,n)!=n) { terminarTransferencia("FLASH_LEITURA","falhou","Falha ao ler bloco para envio.");return; }
    }
    size_t bytes=montarQuadro(quadro,tipo,transferencia.seq,dados,n);
    LoRa.idle();LoRa.onTxDone(nullptr);
    if(!LoRa.beginPacket()||LoRa.write(quadro,bytes)!=bytes) {
      terminarTransferencia("LORA_ENVIO","falhou","Nao foi possivel preparar o quadro no radio.");return;
    }
    strcpy(idPacoteLoRa,transferencia.ensaio);inicioPacoteLoRa=millis();loraTransmitindo=true;
    if(LoRa.endPacket(true)!=1) { terminarTransferencia("LORA_ENVIO","falhou","Falha ao iniciar TX assincrono.");return; }
    digitalWrite(LED_LORA,HIGH);transferencia.ultimoTX=millis();transferencia.tentativas++;transferencia.quadrosTX++;
    transferencia.etapa=Transferencia::TX;return;
  }
  if(transferencia.etapa==Transferencia::TX) {
    if(loraTransmitindo)return;
    digitalWrite(LED_LORA,LOW);LoRa.onTxDone(nullptr);LoRa.parsePacket();
    transferencia.espera=millis();transferencia.etapa=Transferencia::ESPERANDO;
    if(!transferencia.iniciado) { transferencia.iniciado=true;eventoTransferencia("TRANSFERENCIA_INICIADA","iniciado","Quadro INICIO transmitido localmente; aguardando receptor."); }
    if(transferencia.seq==transferencia.blocos+1&&!transferencia.localConcluido) {
      transferencia.localConcluido=true;
      eventoTransferencia("ENVIO_LOCAL_CONCLUIDO","andamento","Todos os dados e FIM transmitidos localmente; recebimento integral ainda nao confirmado.");
    }
    return;
  }
  if(transferencia.etapa==Transferencia::ESPERANDO) {
    receberControleTransferencia();
    if(enviandoGravado&&transferencia.etapa==Transferencia::ESPERANDO&&millis()-transferencia.espera>=ESPERA_ACK_MS)repetirQuadro();
  }
}
