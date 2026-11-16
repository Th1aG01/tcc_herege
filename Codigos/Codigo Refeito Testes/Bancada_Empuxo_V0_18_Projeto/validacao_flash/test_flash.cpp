#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#define BANCADA_SPI_NOR
#define BANCADA_PROVISIONAMENTO
#define loop loopFirmware
#include "../Bancada_Empuxo_V0_18/Bancada_Empuxo_V0_18.ino"
#undef loop
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(0)
void tick(){LoRa.tick();loopFirmware();}
void drain(){for(int i=0;i<2000&&tamanhoTX;i++)atenderSaidaUSB();}
void cmd(uint32_t id,const char *nome,const std::string &extra=""){
  JsonDocument d;d["protocolo"]=2;d["tipo"]="comando";d["id"]=id;d["sessao"]=sessao;d["comando"]=nome;
  if(!extra.empty()){JsonDocument e;CHECK(!deserializeJson(e,extra));for(JsonPair kv:e.as<JsonObject>())d[kv.key()]=kv.value();}
  std::string s;serializeJson(d,s);Serial.feed(s+"\n");
  for(int i=0;i<2000&&Serial.available();i++){ReceberComando();atenderSaidaUSB();}CHECK(!Serial.available());drain();
}
void base(bool escrever=true){
  pins[13]=pins[21]=HIGH;FlashEnsaios.nor.escrita=true;
  CHECK(FlashEnsaios.prepararVazia(true));CHECK(FlashEnsaios.end());
  FlashEnsaios.nor.escrita=escrever;setup();
  for(int i=0;i<10;i++){tick();}
  drain();CHECK(memoriaPronta&&taraPronta);
  cmd(1,"CONFIGURAR_CALIBRACAO","{\"coeficiente\":100}");cmd(2,"TARA");
  for(int i=0;i<10;i++){tick();}
  drain();CHECK(taraPronta);
}
void sample(){fakeMillis+=100;celula.value=2500;tick();drain();}
std::string record(uint32_t id,bool vivo=false){
  cmd(id,vivo?"INICIAR_LORA":"INICIAR");CHECK(ensaioAtivo);
  std::string uid=resumo.id;for(int i=0;i<8;i++)sample();
  cmd(id+1,"FINALIZAR");CHECK(resumo.fechamentoConfirmado);return uid;
}
std::vector<uint8_t> bytes(const std::string &id){
  char p[64];caminhoEnsaio(id.c_str(),p,sizeof(p));auto f=FlashEnsaios.open(p);
  CHECK(f);std::vector<uint8_t>b(f.size());CHECK(f.read(b.data(),b.size())==b.size());CHECK(f.close());return b;
}
void snapshot(const char *env){
  const char *p=std::getenv(env);CHECK(p);std::ofstream out(p,std::ios::binary);
  out.write((const char*)SPI.bytes.data(),SPI.bytes.size());CHECK(out.good());
}
void load(const char *env){
  const char *p=std::getenv(env);CHECK(p);std::ifstream in(p,std::ios::binary);
  in.read((char*)SPI.bytes.data(),SPI.bytes.size());CHECK(in.good());
}
void remount(){CHECK(FlashEnsaios.end());CHECK(FlashEnsaios.begin());}
std::string controle(uint8_t tipo,uint32_t seq){
  std::string b(90,'\0');b[0]='B';b[1]='T';b[2]=1;b[3]=tipo;
  memcpy(&b[4],transferencia.id,32);memcpy(&b[36],transferencia.ensaio,32);
  auto le=[&](size_t a,uint32_t v){for(int i=0;i<4;i++)b[a+i]=char(v>>(8*i));};
  le(68,seq);le(72,transferencia.blocos);le(76,transferencia.totalBytes);le(80,transferencia.crcArquivo);le(86,crc32(b.data(),86));return b;
}
int main(int argc,char **argv){
  CHECK(argc>=2);std::string caso=argv[1];pins[13]=pins[21]=HIGH;
  if(caso=="jedec_ausente"||caso=="jedec_incorreto"||caso=="busy"||caso=="boot_vazio"||caso=="boot_desconhecido"){
    if(caso=="jedec_ausente")SPI.absent=true;
    if(caso=="jedec_incorreto")SPI.jedec=0xc84015;
    if(caso=="busy")SPI.busy=true;
    if(caso=="boot_desconhecido")SPI.bytes[0]=0x42;
    auto antes=SPI.bytes;setup();drain();CHECK(!memoriaPronta);
    CHECK(SPI.bytes==antes&&SPI.programs==0&&SPI.erases==0);
    cmd(3,"STATUS");cmd(4,"ESPACO");cmd(5,"INICIAR");CHECK(!ensaioAtivo);
    CHECK(!FlashEnsaios.begin(true));
  }else if(caso=="provisionamento_protegido"){
    FlashEnsaios.nor.escrita=true;SPI.bytes[1500000]=0;auto antes=SPI.bytes;
    CHECK(!FlashEnsaios.prepararVazia(false));CHECK(!FlashEnsaios.prepararVazia(true));
    CHECK(SPI.bytes==antes&&!SPI.erases&&!SPI.programs);
  }else if(caso=="driver_paginas"||caso=="driver_programa_falha"||caso=="driver_erase_falha"||caso=="driver_wel"||caso=="driver_protegido"||caso=="driver_limites"||caso=="driver_flush"){
    auto &n=FlashEnsaios.nor;n.escrita=true;CHECK(n.begin());uint8_t b[600],r[600];memset(b,0x5a,sizeof(b));
    if(caso=="driver_paginas"){
      CHECK(n.program(250,b,sizeof(b)));CHECK(SPI.programs==4);CHECK(n.read(250,r,sizeof(r)));CHECK(!memcmp(b,r,sizeof(b)));
      CHECK(n.erase(0));CHECK(n.read(0,r,sizeof(r)));for(auto v:r)CHECK(v==0xff);
    }else if(caso=="driver_programa_falha") {SPI.failProgram=true;CHECK(!n.program(0,b,256));CHECK(n.falha);}
    else if(caso=="driver_erase_falha") {SPI.bytes[0]=0;SPI.failErase=true;CHECK(!n.erase(0));CHECK(SPI.bytes[0]==0);}
    else if(caso=="driver_wel") {SPI.failWel=true;CHECK(!n.program(0,b,256));CHECK(!SPI.programs);}
    else if(caso=="driver_protegido") {SPI.protection=4;CHECK(!n.erase(0));CHECK(!SPI.erases&&SPI.protection==4);}
    else if(caso=="driver_flush") {SPI.busy=true;CHECK(!n.sync());CHECK(n.falha);}
    else {CHECK(!n.program(2097150,b,4));CHECK(!n.erase(1));CHECK(!n.read(UINT32_MAX,r,1));CHECK(!SPI.erases&&!SPI.programs);}
  }else if(caso=="somente_leitura"){
    base(false);auto antes=SPI.bytes;auto mut=SPI.mutation;
    cmd(3,"INICIAR");CHECK(!ensaioAtivo);CHECK(!FlashEnsaios.remove("/ensaios"));
    CHECK(!FlashEnsaios.open("/ensaios/x.ben",FLASH_NOVO));CHECK(!FlashEnsaios.mkdir("/x"));
    cmd(4,"STATUS");cmd(5,"ESPACO");CHECK(antes==SPI.bytes&&mut==SPI.mutation);
  }else if(caso=="persistencia_gravar"){
    base();record(3);record(5);
    cmd(7,"INICIAR");for(int i=0;i<12;i++)sample();CHECK(arquivoEnsaio.flush());
    snapshot("NOR_DISK"); // imagem antes do fechamento: simula corte, sem rodape.
    arquivoEnsaio.close();
  }else if(caso=="persistencia_ler"){
    load("NOR_DISK");auto antes=SPI.bytes;setup();drain();CHECK(memoriaPronta&&!ensaioAtivo&&!calibracaoConfigurada);
    auto dir=FlashEnsaios.open("/ensaios");bool d=false;int n=0,concluidos=0,incompletos=0;
    for(;;){auto p=dir.getNextFileName(&d);if(p.empty())break;char id[32];CHECK(idDoArquivo(p.c_str(),id));auto info=inspecionarArquivo(id);CHECK(info.cabecalhoOk);++n;
      if(info.fechado)++concluidos;else{++incompletos;CHECK(info.slots==12);}
      auto f=FlashEnsaios.open(p.c_str());for(uint32_t i=1;i<=info.slots;i++){Payload v;CHECK(lerAmostra(f,info.cab,i,v));CHECK(v.empuxo==15&&v.fatorCalibracao==100);}f.close();
    }dir.close();CHECK(n==3&&concluidos==2&&incompletos==1);CHECK(antes==SPI.bytes&&!SPI.programs&&!SPI.erases);
  }else if(caso=="multiplos_crc_exclusao"||caso=="lora_spi"||caso=="usb_ausente"||caso=="flush_falha"||caso=="capacidade_cheia"||caso=="colisao"||caso=="corrupcao_amostra"||caso=="corte_preparar"||caso=="recuperar_superbloco"){
    base();auto a=record(3,true);auto original=bytes(a);CHECK(LittleFS.files.empty());
    if(caso=="multiplos_crc_exclusao"){
      auto b=record(5);remount();CHECK(bytes(a)==original);CHECK(inspecionarArquivo(a.c_str()).fechado);
      cmd(7,"INICIAR");CHECK(!strcmp(excluirArquivo(resumo.id),"ENSAIO_PROTEGIDO"));cmd(8,"FINALIZAR");
      char p[64];caminhoEnsaio(b.c_str(),p,sizeof(p));CHECK(FlashEnsaios.remove(p));remount();CHECK(bytes(a)==original&&!FlashEnsaios.exists(p));
    }else if(caso=="recuperar_superbloco"){
      std::fill(SPI.bytes.begin(),SPI.bytes.begin()+4096,0);remount();CHECK(bytes(a)==original);
    }else if(caso=="colisao"){
      char p[64];caminhoEnsaio(a.c_str(),p,sizeof(p));CHECK(!FlashEnsaios.open(p,FLASH_NOVO));CHECK(bytes(a)==original);
    }else if(caso=="usb_ausente"){
      Serial.capacidadeTX=0;cmd(5,"INICIAR");for(int i=0;i<200;i++)sample();CHECK(amostrasGravadas==200);cmd(6,"FINALIZAR");CHECK(resumo.fechamentoConfirmado);
    }else if(caso=="flush_falha"){
      cmd(5,"INICIAR");sample();SPI.failProgram=true;for(int i=0;i<20&&ensaioAtivo;i++)sample();CHECK(!ensaioAtivo&&!resumo.fechamentoConfirmado);
      SPI.failProgram=false;remount();CHECK(bytes(a)==original);
    }else if(caso=="capacidade_cheia"){
      auto f=FlashEnsaios.open("/enchimento",FLASH_NOVO);CHECK(f);uint8_t b[4096];memset(b,0xa5,sizeof(b));size_t total=0;
      for(int i=0;i<600;i++){if(f.write(b,sizeof(b))!=sizeof(b))break;total+=sizeof(b);if(!f.flush())break;}CHECK(total>1900000&&total<2097152);f.close();
      cmd(5,"INICIAR");CHECK(!ensaioAtivo);CHECK(bytes(a)==original);CHECK(SPI.erases>0);
    }else if(caso=="corrupcao_amostra"){
      // Corrompe bytes de uma amostra no meio fisico, sem alterar metadados LittleFS.
      auto pattern=original.begin()+392;auto it=std::search(SPI.bytes.begin(),SPI.bytes.end(),pattern,pattern+64);CHECK(it!=SPI.bytes.end());
      *(it+8)^=1;remount();auto info=inspecionarArquivo(a.c_str());char p[64];caminhoEnsaio(a.c_str(),p,sizeof(p));auto f=FlashEnsaios.open(p);Payload v;
      CHECK(!lerAmostra(f,info.cab,1,v));f.close();CHECK(FlashEnsaios.exists(p));
    }else if(caso=="corte_preparar"||caso=="recuperar_superbloco"){
      cmd(5,"INICIAR");sample();CHECK(arquivoEnsaio.flush());snapshot("NOR_BASE");arquivoEnsaio.close();
    }else{
      // Operacao Flash enquanto o radio esta em TX e consultas ao IRQ no mesmo barramento.
      CHECK(!LoRa.packets.empty());while(loraTransmitindo){CHECK(bytes(a)==original);fakeMillis+=50;tick();drain();}cmd(5,"ENVIAR",std::string("{\"ensaio_id\":\"")+a+"\"}");
      for(int i=0;i<5000&&enviandoGravado;i++){
        fakeMillis+=50;tick();drain();
        if(loraTransmitindo)CHECK(bytes(a)==original);
        if(transferencia.etapa==Transferencia::ESPERANDO)LoRa.feedPacket(controle(transferencia.seq==transferencia.blocos+1?BT_CONFIRMAR:BT_ACK,transferencia.seq));
      }
      CHECK(transferencia.confirmado&&!enviandoGravado);CHECK(bytes(a)==original);
      std::vector<uint8_t> reconstruido(original.size());std::vector<bool> visto(transferencia.blocos,false);
      for(auto &q:LoRa.packets){
        if(q.size()<90||q[0]!='B'||q[1]!='T')continue;
        auto p=(const uint8_t*)q.data();size_t n=p[84]|(size_t(p[85])<<8);
        CHECK(q.size()==n+90&&get32(p+86+n)==crc32(p,86+n));
        if(p[3]!=BT_DADOS)continue;
        uint32_t seq=get32(p+68);CHECK(seq&&seq<=visto.size());size_t off=(seq-1)*128;
        CHECK(off+n<=reconstruido.size());std::copy(p+86,p+86+n,reconstruido.begin()+off);visto[seq-1]=true;
      }
      for(bool v:visto){CHECK(v);}CHECK(reconstruido==original);
      CHECK(LoRa.txDone==nullptr&&SPI.transactions>1000&&SPI.radioTransactions>20&&pins[13]==HIGH&&pins[21]==HIGH);
    }
  }else if(caso=="corte_escrever"){
    CHECK(argc==3);load("NOR_BASE");FlashEnsaios.nor.escrita=true;setup();CHECK(memoriaPronta);
    // Criacao/gravação real, interrupcao em cada operacao program/erase selecionada.
    SPI.cutAfter=std::atoi(argv[2]);auto f=FlashEnsaios.open("/ensaios/CORTE.ben",FLASH_NOVO);
    uint8_t b[8192];memset(b,0x37,sizeof(b));if(f){f.write(b,sizeof(b));f.flush();}
    snapshot("NOR_DISK");f.close();
  }else if(caso=="corte_ler"){
    load("NOR_DISK");auto antes=SPI.bytes;setup();CHECK(memoriaPronta);// IDs recuperados pelo catalogo.
    auto dir=FlashEnsaios.open("/ensaios");bool d=false;int validos=0;
    for(;;){auto p=dir.getNextFileName(&d);if(p.empty())break;char id[32];if(!idDoArquivo(p.c_str(),id))continue;auto v=inspecionarArquivo(id);if(v.fechado){++validos;CHECK(v.fim.amostras==8);}}
    CHECK(validos==1);dir.close();CHECK(antes==SPI.bytes&&!SPI.programs&&!SPI.erases);
  }else CHECK(false);
  drain();std::cout<<Serial.output;std::cerr<<"PASS "<<caso<<" programs="<<SPI.programs<<" erases="<<SPI.erases<<" transactions="<<SPI.transactions<<'\n';
}
