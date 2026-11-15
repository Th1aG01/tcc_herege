#include <iostream>
#include <sstream>
#include <cstdlib>
#include <fstream>
#define loop loopFirmware
#include "../Bancada_Empuxo_V0_16/Bancada_Empuxo_V0_16.ino"
#undef loop
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<'\n';exit(1);}}while(0)
void loop(){LoRa.tick();loopFirmware();}
void flush(){for(int i=0;i<1000&&tamanhoTX;i++)atenderSaidaUSB();}
void input(const std::string& s){Serial.feed(s);for(int i=0;i<2000&&Serial.available();i++){ReceberComando();atenderSaidaUSB();}CHECK(!Serial.available());flush();}
std::string request(uint32_t id,const char* command,bool session=true){
  JsonDocument d;d["protocolo"]=2;d["tipo"]="comando";d["id"]=id;d["comando"]=command;
  if(session)d["sessao"]=sessao;
  std::string text;serializeJson(d,text);return text+"\n";
}
void cmd(uint32_t id,const char* name){input(request(id,name));}
std::vector<JsonDocument> messages(){
  flush();std::vector<JsonDocument> out;std::istringstream stream(Serial.output);std::string line;
  while(std::getline(stream,line)){JsonDocument d;CHECK(!deserializeJson(d,line));CHECK(d["protocolo"]==2);out.push_back(std::move(d));}
  return out;
}
int count(const char* key,const char* value){int n=0;for(auto&d:messages())if(d[key]==value)n++;return n;}
JsonDocument last(const char* type){auto v=messages();for(auto i=v.rbegin();i!=v.rend();++i)if((*i)["tipo"]==type)return *i;CHECK(false);return JsonDocument();}
void config(uint32_t id,const char* literal){auto s=request(id,"CONFIGURAR_CALIBRACAO");s.insert(1,std::string("\"coeficiente\":")+literal+",");input(s);}
void tare(uint32_t id){cmd(id,"TARA");for(int i=0;i<10;i++){loop();}flush();}
void sample(){fakeMillis+=100;celula.value=2500;loop();flush();}
void record(){cmd(1,"INICIAR");sample();sample();sample();cmd(2,"FINALIZAR");}

void recuperarResumoTeste(){
  std::string path=caminhoGravacao;
  char id[32];if(!idDoArquivo(path.c_str(),id)){strcpy(id,ID_LEGADO);}
  auto info=inspecionarArquivo(id);
  strcpy(resumo.id,id);resumo.amostras=info.slots;resumo.duracao_ms=info.fim.duracao_ms;
  resumo.fechamentoConfirmado=info.fechado;
  resumo.resultado=info.fechado?(info.fim.resultado==1?"CONCLUIDO":"FALHOU"):"INCOMPLETO";
  strcpy(resumo.motivo,info.fechado?info.fim.motivo:"SEM_FECHAMENTO_VALIDO");
}

void command(uint32_t id,const char* name,const std::string& params){
  auto s=request(id,name);if(!params.empty())s.insert(1,params+",");input(s);
}
std::string idParam(const std::string& id){JsonDocument d;d["ensaio_id"]=id;std::string s;serializeJson(d,s);return s.substr(1,s.size()-2);}
std::string idsParam(std::initializer_list<std::string> ids){JsonDocument d;auto a=d["ensaio_ids"].to<JsonArray>();for(auto&id:ids)a.add(id);std::string s;serializeJson(d,s);return s.substr(1,s.size()-2);}
std::shared_ptr<FileData> arquivoDe(const std::string& id){char path[64];caminhoEnsaio(id.c_str(),path,sizeof(path));auto it=LittleFS.files.find(path);return it==LittleFS.files.end()?nullptr:it->second;}
std::string novoEnsaio(uint32_t id){cmd(id,"INICIAR");CHECK(ensaioAtivo);std::string uid=resumo.id;sample();sample();sample();cmd(id+1,"FINALIZAR");CHECK(resumo.fechamentoConfirmado);return uid;}
void drainCatalog(){for(int i=0;i<10000&&operacaoCatalogo.tipo!=OperacaoCatalogo::NENHUMA;i++){atualizarCatalogo();atenderSaidaUSB();}flush();CHECK(operacaoCatalogo.tipo==OperacaoCatalogo::NENHUMA);}
void listar(uint32_t id,unsigned limite=4,const std::string& apos="",uint32_t revisao=0){
  JsonDocument p;p["limite"]=limite;if(!apos.empty())p["apos"]=apos;if(revisao)p["revisao"]=revisao;
  std::string s;serializeJson(p,s);command(id,"LISTAR_ENSAIOS",s.substr(1,s.size()-2));drainCatalog();
}
void salvarDisco(){
  const char* path=std::getenv("TESTE_DISCO");CHECK(path);std::ofstream out(path,std::ios::binary);CHECK(out);
  uint32_t n=LittleFS.files.size();out.write((const char*)&n,4);
  for(auto&kv:LittleFS.files){uint32_t len=kv.first.size(),bytes=kv.second->bytes.size();out.write((const char*)&len,4);out.write(kv.first.data(),len);out.write((const char*)&bytes,4);out.write((const char*)kv.second->bytes.data(),bytes);}
  CHECK(out.good());
}
void carregarDisco(){
  const char* path=std::getenv("TESTE_DISCO");CHECK(path);std::ifstream in(path,std::ios::binary);CHECK(in);
  uint32_t n;in.read((char*)&n,4);CHECK(n<100);
  for(uint32_t i=0;i<n;i++){
    uint32_t len,bytes;in.read((char*)&len,4);CHECK(len<128);std::string name(len,' ');in.read(name.data(),len);in.read((char*)&bytes,4);CHECK(bytes<100000);
    auto file=std::make_shared<FileData>();file->bytes.resize(bytes);in.read((char*)file->bytes.data(),bytes);LittleFS.files[name]=file;
  }CHECK(in.good());LittleFS.dirExists=true;
}

int main(int argc,char**argv){
  CHECK(argc==2);std::string test=argv[1];
  if(test=="sem_hx")celula.ready=false;
  if(test=="sem_lora")LoRa.ready=false;
  if(test=="sem_flash")LittleFS.ready=false;
  if(test=="ar_reboot_ler")carregarDisco();
  setup();
  if(test!="boot_responsivo"&&test!="sem_hx"&&test!="cal_boot_ausente")for(int i=0;i<10;i++){loop();}
  flush();Serial.output.clear();
  // Fixture dos testes de regressao: calibracao valida ja estabelecida.
  if(test.rfind("cal_",0)!=0&&test!="ar_reboot_ler")calibracaoConfigurada=true;

  if(test=="identidade"){
    input(request(0,"IDENTIFICAR",false));auto d=last("resposta");
    CHECK(d["firmware"]=="0.16"&&d["id"]==0&&d["baud"]==115200);
    CHECK(d["sessao"].as<JsonString>().size()==16&&d["dispositivo"]=="ESP32-AABBCCDDEEFF");
    CHECK(maiorId==0);cmd(1,"STATUS");d=last("resposta");
    CHECK(d["sensor"]["tara_pronta"]==true&&d["calibracao"]["unidade"]=="kgf");
    CHECK(d["armazenamento"]["pronto"]==true&&d["radio"]["pronto"]==true);
  }else if(test=="fragmentos"){
    auto r=request(1,"INICIAR");for(size_t i=0;i<r.size()-1;i++){input(r.substr(i,1));CHECK(!ensaioAtivo);fakeMillis+=5;}
    input("\n");CHECK(ensaioAtivo&&LittleFS.writeOpens==1);
  }else if(test=="antigos"){
    input("i\na\nf\nl\ne\nc\nINICIAR\n");CHECK(!ensaioAtivo&&LittleFS.writeOpens==0);
    cmd(1,"IFALEC");CHECK(count("codigo","COMANDO_DESCONHECIDO")==1);CHECK(!ensaioAtivo);
  }else if(test=="malformados"){
    for(auto s:{"{\"id\":1,}\n","{'id':1}\n","{id:1}\n","[]\n","{} lixo\n","{}{}\n","{\"id\":01}\n","{\"id\":1e0}\n","{\"id\":1.0}\n","{\"id\":-1}\n","{\"id\":true}\n","{\"id\":{}}\n","{\"id\":1,\"id\":2}\n","{\"x\":\"\\q\"}\n"})input(s);
    CHECK(count("tipo","erro")==14&&maiorId==0&&LittleFS.writeOpens==0);
  }else if(test=="lixo_apos_json"){
    auto s=request(1,"INICIAR");s.pop_back();input(s+"x\n");input(s+s+"\n");CHECK(!ensaioAtivo&&maiorId==0);
  }else if(test=="duplicadas"){
    auto s=request(1,"INICIAR");s.insert(1,"\"id\":1,");input(s);CHECK(!ensaioAtivo&&maiorId==0);
    s=request(1,"INICIAR");s.insert(1,"\"\\u0069d\":2,");input(s);CHECK(!ensaioAtivo);
  }else if(test=="timeout_rx"){
    input("{\"protocolo\":2,");fakeMillis+=1000;ReceberComando();flush();
    CHECK(count("codigo","RX_TIMEOUT")==1);input(request(1,"INICIAR"));CHECK(!ensaioAtivo);
    cmd(1,"INICIAR");CHECK(ensaioAtivo);
  }else if(test=="limite_rx"){
    auto s=request(1,"STATUS");s.pop_back();s.append(512-s.size(),' ');input(s+"\n");CHECK(last("resposta")["id"]==1);
    input(std::string(513,' ')+request(2,"INICIAR"));CHECK(count("codigo","RX_TAMANHO")==1&&!ensaioAtivo);
    cmd(2,"INICIAR");CHECK(ensaioAtivo);
  }else if(test=="nul_controle"){
    input(std::string("{\0",2)+request(1,"INICIAR"));CHECK(!ensaioAtivo);
    auto s=request(1,"INICIAR");s.replace(s.find("INICIAR"),7,"INICIAR\\u0000");input(s);CHECK(!ensaioAtivo);
    CHECK(count("tipo","erro")==2);
  }else if(test=="crlf_burst"){
    auto s=request(1,"INICIAR");s.insert(s.size()-1,"\r");input(" \r\n\n"+s+request(2,"FINALIZAR"));
    CHECK(!ensaioAtivo&&LittleFS.writeOpens==1&&count("tipo","resposta")==2);
  }else if(test=="sessao_errada"){
    auto s=request(1,"INICIAR");s.replace(s.find(sessao),16,"0000000000000000");input(s);
    input(request(2,"INICIAR",false));CHECK(count("codigo","SESSAO_INVALIDA")==2&&!ensaioAtivo&&maiorId==0);
  }else if(test=="esquema"){
    input("{\"protocolo\":2,\"tipo\":\"comando\",\"comando\":\"INICIAR\"}\n");
    auto s=request(1,"INICIAR");s.insert(1,"\"extra\":1,");input(s);
    s=request(1,"INICIAR");s.replace(s.find("\"protocolo\":2"),13,"\"protocolo\":1");input(s);
    input(request(0,"INICIAR",false));cmd(1,"iniciar");
    input("{\"id\":4294967296}\n");CHECK(count("tipo","erro")==6&&!ensaioAtivo&&maiorId==0);
  }else if(test=="dedup_inicio"){
    cmd(1,"INICIAR");sample();auto before=LittleFS.saved->bytes;
    auto s=request(1,"INICIAR");cmd(1,"INICIAR");CHECK(ensaioAtivo&&LittleFS.writeOpens==1&&LittleFS.saved->bytes==before);
    cmd(2,"FINALIZAR");Serial.output.clear();cmd(1,"INICIAR");
    CHECK(!ensaioAtivo&&LittleFS.writeOpens==1);CHECK(last("resposta")["fase"]=="aceito");
    CHECK(last("evento")["id"]==1&&last("evento")["fase"]=="concluido");
  }else if(test=="conflito"){
    cmd(1,"INICIAR");cmd(1,"FINALIZAR");CHECK(ensaioAtivo&&count("codigo","ID_CONFLITO")==1);
  }else if(test=="expirado"){
    cmd(1,"INICIAR");cmd(2,"FINALIZAR");for(uint32_t i=3;i<20;i++)cmd(i,"STATUS");
    cmd(1,"INICIAR");CHECK(!ensaioAtivo&&LittleFS.writeOpens==1&&count("codigo","ID_EXPIRADO")==1);
  }else if(test=="pendente_retido"){
    cmd(1,"INICIAR");for(uint32_t i=2;i<20;i++)cmd(i,"STATUS");
    cmd(1,"INICIAR");CHECK(LittleFS.writeOpens==1&&last("resposta")["fase"]=="aceito");
    sample();cmd(20,"FINALIZAR");Serial.output.clear();cmd(1,"INICIAR");CHECK(last("evento")["fase"]=="concluido");
  }else if(test=="dedup_alternar"){
    cmd(1,"INICIAR");cmd(2,"LORA_ALTERNAR");CHECK(transmitirLoRa);cmd(2,"LORA_ALTERNAR");CHECK(transmitirLoRa);
  }else if(test=="dedup_erro"){
    cmd(1,"LORA_LIGAR");cmd(2,"INICIAR");cmd(1,"LORA_LIGAR");CHECK(!transmitirLoRa&&count("codigo","ENSAIO_INATIVO")==2);
  }else if(test=="status_snapshot"){
    cmd(1,"STATUS");cmd(2,"INICIAR");cmd(1,"STATUS");CHECK(last("resposta")["estado"]=="OCIOSO");
    cmd(3,"STATUS");CHECK(last("resposta")["estado"]=="PREPARANDO");sample();cmd(4,"STATUS");CHECK(last("resposta")["estado"]=="ADQUIRINDO");
  }else if(test=="envio_conclusao"){
    record();cmd(3,"ENVIAR");CHECK(last("resposta")["fase"]=="aceito");CHECK(count("codigo","ENVIO_CONCLUIDO")==0);
    for(int i=0;i<30&&enviandoGravado;i++){fakeMillis+=100;loop();}
    flush();CHECK(!enviandoGravado&&LoRa.packets.size()==3&&last("evento")["id"]==3);
    cmd(3,"ENVIAR");CHECK(!enviandoGravado&&LoRa.packets.size()==3&&last("evento")["fase"]=="concluido");
  }else if(test=="cancelamento"){
    record();cmd(3,"ENVIAR");cmd(4,"CANCELAR");CHECK(!enviandoGravado);
    cmd(3,"ENVIAR");CHECK(last("evento")["fase"]=="cancelado"&&last("evento")["id"]==3);
  }else if(test=="falha_envio"){
    record();cmd(3,"ENVIAR");failRead=true;loop();flush();
    CHECK(!enviandoGravado&&last("evento")["fase"]=="falhou"&&last("evento")["id"]==3);
  }else if(test=="timeout_radio"){
    record();cmd(3,"ENVIAR");loop();LoRa.txDone=nullptr;fakeMillis+=5000;loop();flush();
    CHECK(!enviandoGravado&&!loraPronto&&last("evento")["fase"]=="falhou");
    CHECK(count("codigo","LORA_TX_TIMEOUT")==1);
  }else if(test=="falha_sensor"){
    cmd(1,"INICIAR");sample();celula.ready=false;fakeMillis+=2000;loop();flush();
    CHECK(!ensaioAtivo&&last("evento")["id"]==1&&last("evento")["fase"]=="falhou");
  }else if(test=="falha_flash"){
    cmd(1,"INICIAR");failWrite=true;sample();CHECK(!ensaioAtivo&&last("evento")["id"]==1);
    CHECK(last("evento")["fase"]=="falhou"&&count("tipo","amostra")==0);
  }else if(test=="sem_lora"){
    cmd(1,"INICIAR_LORA");CHECK(!ensaioAtivo&&LittleFS.writeOpens==0);cmd(2,"INICIAR");CHECK(ensaioAtivo);
  }else if(test=="sem_flash"){
    cmd(1,"INICIAR");CHECK(!ensaioAtivo&&count("codigo","FLASH_INDISPONIVEL")==1);
  }else if(test=="sem_hx"){
    input(request(0,"IDENTIFICAR",false));CHECK(fakeMillis==0&&last("resposta")["id"]==0);
    fakeMillis=3000;loop();flush();CHECK(!taraPronta&&count("codigo","HX711_TARA_INICIAL_TIMEOUT")==1);
  }else if(test=="boot_responsivo"){
    CHECK(taraEmAndamento&&fakeMillis==0);cmd(1,"STATUS");CHECK(last("resposta")["sensor"]["tara_em_andamento"]==true);
    cmd(2,"INICIAR");CHECK(!ensaioAtivo);for(int i=0;i<10;i++){loop();}cmd(3,"INICIAR");CHECK(ensaioAtivo);
  }else if(test=="backpressure"){
    cmd(1,"INICIAR");Serial.capacidadeTX=0;
    for(int i=0;i<200;i++){fakeMillis+=10;loop();}
    CHECK(amostrasGravadas==200&&amostrasDescartadasUSB>0&&tamanhoTX<TX_MAX);
    Serial.capacidadeTX=7;flush();CHECK(tamanhoTX==0);cmd(2,"FINALIZAR");CHECK(!ensaioAtivo);
  }else if(test=="controle_congestionado"){
    Serial.capacidadeTX=0;Serial.feed(request(1,"STATUS")+request(2,"STATUS")+request(3,"STATUS")+request(4,"STATUS")+request(5,"STATUS")+request(6,"STATUS")+request(7,"STATUS")+request(8,"STATUS")+request(9,"INICIAR"));
    for(int i=0;i<100;i++){ReceberComando();}CHECK(!ensaioAtivo&&Serial.available());
    Serial.capacidadeTX=64;for(int i=0;i<1000;i++){atenderSaidaUSB();ReceberComando();}
    flush();CHECK(ensaioAtivo&&LittleFS.writeOpens==1&&mensagensDescartadasUSB==0);
  }else if(test=="amostra"){
    payload.fatorCalibracao=1000;cmd(1,"INICIAR");sample();auto d=last("amostra");
    CHECK(d["empuxo_kgf"]==1.5&&d["ensaio_id"]==resumo.id&&d["indice"]==1);
  }else if(test=="permutacao"){
    cmd(1,"INICIAR");
    input(std::string("{ \"sessao\":\"")+sessao+"\",\"comando\":\"INICIAR\",\"id\":1,\"tipo\":\"comando\",\"protocolo\":2}\n");
    CHECK(LittleFS.writeOpens==1&&count("tipo","resposta")==2);
  }else if(test=="fuzz"){
    uint32_t seed=12345;
    const std::string chars="{}[]\"\\:,0123456789ifalec? abcdef\r\t";
    for(int k=0;k<1000;k++){
      seed=seed*1664525U+1013904223U;
      size_t n=seed%600;std::string data;
      for(size_t j=0;j<n;j++){seed=seed*1664525U+1013904223U;data+=chars[seed%chars.size()];}
      input(data+"\n");
    }
    CHECK(!ensaioAtivo&&LittleFS.writeOpens==0&&maiorId==0);
    cmd(1,"INICIAR");CHECK(ensaioAtivo);
  }else if(test=="configuracao"){
    CHECK(Serial.baud==115200&&celula.dt==33&&celula.sck==32);
    CHECK(SPI.args==std::vector<int>({18,19,23,21})&&LoRa.pins==std::vector<int>({21,14,26}));
    CHECK(LoRa.frequency==915E6&&LoRa.sync==145&&LoRa.power==2&&LoRa.spiHz==1000000);
    CHECK(modes.size()==2&&!LittleFS.formatFlag);
  }else if(test=="cal_padrao"){
    cmd(1,"CONSULTAR_CALIBRACAO");auto d=last("resposta");
    CHECK(d["calibracao"]["coeficiente"]==1&&d["calibracao"]["configurada"]==false&&d["calibracao"]["valida"]==false);
    CHECK(d["tara"]["pronta"]==true&&!calibracaoConfigurada);
    cmd(2,"INICIAR");CHECK(!ensaioAtivo&&LittleFS.writeOpens==0&&count("codigo","CALIBRACAO_NAO_CONFIGURADA")==1);
  }else if(test=="cal_configurar"){
    config(1,"1234.5");CHECK(payload.fatorCalibracao==1234.5&&calibracaoConfigurada&&!taraPronta);
    auto d=last("resposta");CHECK(d["calibracao"]["valida"]==true&&d["calibracao"]["pronta_para_uso"]==false);
    cmd(2,"INICIAR");CHECK(!ensaioAtivo&&count("codigo","TARA_NAO_PRONTA")==1);
    tare(3);cmd(4,"INICIAR");CHECK(ensaioAtivo);
  }else if(test=="cal_um_explicito"){
    config(1,"1.0");CHECK(calibracaoValida()&&!taraPronta);tare(2);CHECK(taraPronta);
    cmd(3,"INICIAR");CHECK(ensaioAtivo);
  }else if(test=="cal_sinais_formula"){
    config(1,"1e3");celula.value=1200;tare(2);cmd(3,"INICIAR");sample();
    CHECK(fabs(last("amostra")["empuxo_kgf"].as<double>()-(1.3))<1e-6 && fabs(payload.empuxo-(1.3))<1e-12);cmd(4,"FINALIZAR");
    config(5,"-1000.0");celula.value=1200;tare(6);cmd(7,"INICIAR");sample();
    CHECK(fabs(last("amostra")["empuxo_kgf"].as<double>()-(-1.3))<1e-6 && fabs(payload.empuxo-(-1.3))<1e-12);
  }else if(test=="cal_invalidos"){
    config(1,"1000");tare(2);uint32_t id=3;
    for(auto literal:{"0","-0.0","\"1000\"","\"NaN\"","\"Infinity\"","NaN","Infinity","1e309","-1e309","1e-999","true","null"}){
      config(id++,literal);CHECK(payload.fatorCalibracao==1000&&calibracaoConfigurada&&taraPronta);
    }
    CHECK(count("tipo","erro")==12);
  }else if(test=="cal_extremos_finitos"){
    config(1,"1e-300");CHECK(calibracaoValida()&&payload.fatorCalibracao>0);
    config(2,"-1e300");CHECK(calibracaoValida()&&payload.fatorCalibracao<0);
  }else if(test=="cal_offset_unidade"){
    auto s=request(1,"CONFIGURAR_CALIBRACAO");s.insert(1,"\"coeficiente\":1000,\"offset\":20,");input(s);
    s=request(2,"CONFIGURAR_CALIBRACAO");s.insert(1,"\"coeficiente\":1000,\"unidade\":\"N\",");input(s);
    CHECK(!calibracaoConfigurada&&count("codigo","CAMPO_DESCONHECIDO")==2);
    cmd(3,"CONFIGURAR_CALIBRACAO");CHECK(!calibracaoConfigurada);
  }else if(test=="cal_dedup_parametro"){
    config(1,"1000");tare(2);config(1,"1e3");CHECK(taraPronta);
    config(1,"-1000");CHECK(payload.fatorCalibracao==1000&&taraPronta&&count("codigo","ID_CONFLITO")==1);
    config(3,"1000");CHECK(!taraPronta);
  }else if(test=="cal_bloqueio_aquisicao"){
    config(1,"1000");tare(2);cmd(3,"INICIAR");long old=celula.offset;
    config(4,"-2000");cmd(5,"TARA");CHECK(ensaioAtivo&&payload.fatorCalibracao==1000&&taraPronta&&celula.offset==old);
    CHECK(count("codigo","ENSAIO_ATIVO")==2);
    cmd(6,"FINALIZAR");config(4,"-2000");CHECK(payload.fatorCalibracao==1000);
  }else if(test=="cal_tara_assincrona"){
    config(1,"1000");cmd(2,"TARA");CHECK(taraEmAndamento&&!taraPronta&&last("resposta")["fase"]=="aceito");
    for(int i=0;i<4;i++){loop();}cmd(3,"STATUS");CHECK(last("resposta")["tara"]["em_andamento"]==true);
    for(int i=0;i<6;i++){loop();}flush();CHECK(taraPronta&&!taraEmAndamento&&last("evento")["id"]==2&&last("evento")["fase"]=="concluido");
    CHECK(last("evento")["calibracao"]["pronta_para_uso"]==true);
  }else if(test=="cal_tara_repetida"){
    config(1,"1000");celula.value=1500;cmd(2,"TARA");for(int i=0;i<4;i++){loop();}
    cmd(2,"TARA");CHECK(leiturasTara==4);for(int i=0;i<6;i++){loop();}flush();
    CHECK(celula.offset==1500&&taraPronta);celula.value=2000;cmd(2,"TARA");CHECK(!taraEmAndamento&&celula.offset==1500);
  }else if(test=="cal_tara_sem_duplo_zero"){
    config(1,"1000");celula.value=1500;tare(2);CHECK(celula.offset==1500);
    celula.value=1800;tare(3);CHECK(celula.offset==1800);
    cmd(4,"INICIAR");sample();CHECK(fabs(last("amostra")["empuxo_kgf"].as<double>()-(0.7))<1e-6 && fabs(payload.empuxo-(0.7))<1e-12);
  }else if(test=="cal_tara_ausente"){
    config(1,"1000");celula.ready=false;cmd(2,"TARA");CHECK(taraEmAndamento&&!taraPronta);
    cmd(3,"CONSULTAR_CALIBRACAO");CHECK(last("resposta")["tara"]["em_andamento"]==true);
    fakeMillis+=3000;loop();flush();CHECK(!taraPronta&&!taraEmAndamento&&last("evento")["fase"]=="falhou"&&last("evento")["id"]==2);
    cmd(4,"STATUS");CHECK(last("resposta")["sensor"]["respondendo"]==false);
    celula.ready=true;celula.value=1800;tare(5);CHECK(taraPronta&&celula.offset==1800);
  }else if(test=="cal_tara_parcial"){
    config(1,"1000");long before=celula.offset;celula.value=1800;celula.remaining=5;cmd(2,"TARA");
    for(int i=0;i<10;i++){loop();}CHECK(!taraPronta&&leiturasTara==5);
    fakeMillis+=3000;loop();flush();CHECK(!taraPronta&&celula.offset==before&&last("evento")["fase"]=="falhou");
  }else if(test=="cal_tara_bloqueia_config"){
    config(1,"1000");cmd(2,"TARA");config(3,"2000");cmd(4,"TARA");
    CHECK(payload.fatorCalibracao==1000&&idTara==2&&count("codigo","TARA_EM_ANDAMENTO")==2);
  }else if(test=="cal_recuperar_sensor"){
    config(1,"1000");tare(2);cmd(3,"INICIAR");sample();celula.ready=false;fakeMillis+=2000;loop();flush();
    CHECK(!ensaioAtivo&&!taraPronta&&calibracaoConfigurada);celula.ready=true;loop();
    cmd(4,"INICIAR");CHECK(!ensaioAtivo);tare(5);cmd(6,"INICIAR");CHECK(ensaioAtivo);
  }else if(test=="cal_envio_tara"){
    config(1,"1000");tare(2);cmd(3,"INICIAR");sample();sample();cmd(4,"FINALIZAR");
    cmd(5,"ENVIAR");cmd(6,"TARA");CHECK(enviandoGravado&&taraEmAndamento&&idEnvio==5&&idTara==6);
    for(int i=0;i<20;i++){fakeMillis+=100;loop();}flush();
    CHECK(!enviandoGravado&&taraPronta&&count("codigo","ENVIO_CONCLUIDO")==1&&count("codigo","TARA_CONCLUIDA")==2);
  }else if(test=="cal_fragmentos"){
    auto s=request(1,"CONFIGURAR_CALIBRACAO");s.insert(1,"\"coeficiente\":-1234.567e-2,");
    for(size_t i=0;i<s.size()-1;i++){input(s.substr(i,1));CHECK(!calibracaoConfigurada);}
    input("\n");CHECK(calibracaoConfigurada&&fabs(payload.fatorCalibracao+12.34567)<1e-10);
  }else if(test=="cal_boot_ausente"){
    CHECK(taraEmAndamento&&!calibracaoConfigurada);input(request(0,"IDENTIFICAR",false));CHECK(fakeMillis==0);
    celula.ready=false;fakeMillis=3000;loop();flush();CHECK(!taraPronta&&!taraEmAndamento);
    config(1,"1000");celula.ready=true;tare(2);cmd(3,"INICIAR");CHECK(ensaioAtivo);
  }else if(test=="cal_status_tamanho"){
    config(4294967290U,"-1.23456789e300");tare(4294967291U);cmd(4294967292U,"STATUS");
    CHECK(count("codigo","JSON_MEMORIA")==0&&last("resposta")["calibracao"]["pronta_para_uso"]==true);
    auto d=last("resposta");CHECK(measureJson(d)<RESPOSTA_MAX);
  }else if(test=="aq_inicio_real"){
    celula.ready=false;cmd(1,"INICIAR");CHECK(ensaioAtivo&&!aquisicaoIniciada&&pins[LED_EXEC]==LOW);
    CHECK(last("resposta")["fase"]=="aceito"&&count("codigo","AQUISICAO_INICIADA")==0);
    cmd(2,"STATUS");CHECK(last("resposta")["aquisicao"]["ativa"]==false&&last("resposta")["aquisicao"]["preparando"]==true);
    celula.ready=true;fakeMillis=777;sample();CHECK(aquisicaoIniciada&&pins[LED_EXEC]==HIGH);
    CHECK(count("codigo","AQUISICAO_INICIADA")==1&&last("amostra")["tempo_ms"]==0);
    sample();CHECK(count("codigo","AQUISICAO_INICIADA")==1&&last("amostra")["tempo_ms"]==100);
    cmd(1,"INICIAR");CHECK(contadorEnsaios==1&&amostrasGravadas==2);
  }else if(test=="aq_ids_distintos"){
    cmd(10,"INICIAR");sample();std::string old=resumo.id;CHECK(resumo.solicitacao==10);
    cmd(11,"FINALIZAR");cmd(12,"INICIAR");sample();CHECK(old!=resumo.id&&resumo.solicitacao==12&&payload.indice==1&&payload.tempo_ms==0);
    cmd(10,"INICIAR");CHECK(contadorEnsaios==2&&LittleFS.writeOpens==2);
  }else if(test=="aq_mesma_medicao"){
    payload.fatorCalibracao=-1000;cmd(1,"INICIAR_LORA");sample();
    CHECK(LoRa.packets.size()==1&&payload.empuxo==-1.5);
    Payload stored={};memcpy(&stored,LittleFS.saved->bytes.data()+sizeof(CabecalhoEnsaio),sizeof(stored));
    CHECK(stored.empuxo==payload.empuxo&&stored.indice==1&&stored.tempo_ms==0&&!strcmp(stored.ensaioId,resumo.id));
    std::string usb;auto d=last("amostra");serializeJson(d,usb);CHECK(usb==LoRa.packets.front());
    std::string live=LoRa.packets.front();cmd(2,"FINALIZAR");
    config(3,"777");cmd(4,"ENVIAR");fakeMillis+=300;celula.ready=false;loop();flush();
    CHECK(LoRa.packets.size()==2&&LoRa.packets.back()==live);
  }else if(test=="aq_frequencias_independentes"){
    LoRa.txDuration=1500;cmd(1,"INICIAR_LORA");
    for(int i=0;i<150;i++){fakeMillis+=10;celula.value+=1;loop();flush();}
    CHECK(amostrasGravadas==150&&LoRa.packets.size()==1&&ensaioAtivo);
    CHECK(payload.indice==150&&payload.tempo_ms==1490);
    cmd(2,"FINALIZAR");CHECK(resumo.fechamentoConfirmado&&resumo.amostras==150);
  }else if(test=="aq_nao_reenvia_ultima"){
    cmd(1,"INICIAR_LORA");sample();celula.ready=false;
    for(int i=0;i<5;i++){fakeMillis+=250;loop();flush();}
    CHECK(LoRa.packets.size()==1&&amostrasGravadas==1&&ensaioAtivo);
  }else if(test=="aq_finalizacao"){
    cmd(1,"INICIAR");sample();sample();sample();fakeMillis+=75;cmd(2,"FINALIZAR");
    CHECK(!ensaioAtivo&&!arquivoEnsaio&&pins[LED_EXEC]==LOW&&flushCount==2);
    CHECK(resumo.duracao_ms==275&&resumo.amostras==3&&resumo.fechamentoConfirmado);
    bool terminal=false;for(auto&d:messages())if(d["codigo"]=="ENSAIO_ENCERRADO"){
      CHECK(d["id"]==1&&d["causado_por"]==2&&d["fase"]=="concluido");
      CHECK(d["ensaio"]["duracao_ms"]==275&&d["ensaio"]["amostras"]==3&&d["ensaio"]["motivo"]=="COMANDO_FINALIZAR");terminal=true;
    }CHECK(terminal);
    auto before=LittleFS.saved->bytes;cmd(2,"FINALIZAR");cmd(3,"FINALIZAR");CHECK(before==LittleFS.saved->bytes&&flushCount==2);
    cmd(4,"STATUS");CHECK(last("resposta")["ensaio"]["resultado"]=="CONCLUIDO");
  }else if(test=="aq_espera_timeout"){
    celula.ready=false;cmd(1,"INICIAR");fakeMillis+=2000;loop();flush();
    CHECK(!ensaioAtivo&&pins[LED_EXEC]==LOW&&count("codigo","AQUISICAO_INICIADA")==0);
    CHECK(!strcmp(resumo.resultado,"FALHOU")&&resumo.amostras==0&&!strcmp(resumo.motivo,"HX711_TIMEOUT"));
    CHECK(resumo.fechamentoConfirmado);cmd(2,"ENVIAR");CHECK(!enviandoGravado&&LoRa.packets.empty());
  }else if(test=="aq_cancelar_preparacao"){
    cmd(1,"INICIAR");cmd(2,"FINALIZAR");CHECK(!resumo.iniciado&&resumo.amostras==0&&resumo.duracao_ms==0);
    CHECK(!strcmp(resumo.motivo,"CANCELADO_ANTES_INICIO")&&!strcmp(resumo.resultado,"FALHOU"));
    CHECK(resumo.fechamentoConfirmado&&count("codigo","AQUISICAO_INICIADA")==0);
    cmd(3,"ENVIAR");CHECK(!enviandoGravado);
  }else if(test=="aq_flash_sem_espaco"){
    record();auto before=LittleFS.saved->bytes;
    LittleFS.capacity=RESERVA_FLASH+sizeof(CabecalhoEnsaio)+sizeof(RegistroAmostra)+sizeof(RodapeEnsaio)-1;
    cmd(3,"INICIAR");CHECK(!ensaioAtivo&&LittleFS.writeOpens==1&&LittleFS.saved->bytes==before);
    CHECK(count("codigo","FLASH_SEM_ESPACO")==1);
  }else if(test=="aq_cabecalho_falha"){
    failWrite=true;cmd(1,"INICIAR");CHECK(!ensaioAtivo&&pins[LED_EXEC]==LOW&&count("codigo","FLASH_CABECALHO")==1);
    CHECK(!resumo.fechamentoConfirmado&&!strcmp(resumo.resultado,"FALHOU"));
    failWrite=false;cmd(2,"ENVIAR");CHECK(!enviandoGravado);
  }else if(test=="aq_amostra_parcial"){
    cmd(1,"INICIAR");sample();failWriteCall=writeCalls+1;sample();
    CHECK(!ensaioAtivo&&resumo.amostras==1&&!resumo.fechamentoConfirmado&&count("tipo","amostra")==1);
    CHECK(!strcmp(resumo.motivo,"FLASH_GRAVACAO"));cmd(2,"ENVIAR");CHECK(!enviandoGravado);
    resumo=ResumoEnsaio();recuperarResumoTeste();CHECK(!strcmp(resumo.resultado,"INCOMPLETO"));
  }else if(test=="aq_rodape_falha"){
    cmd(1,"INICIAR");sample();failWriteCall=writeCalls+1;cmd(2,"FINALIZAR");
    CHECK(!resumo.fechamentoConfirmado&&!strcmp(resumo.resultado,"FALHOU")&&!strcmp(resumo.motivo,"FLASH_FECHAMENTO"));
    CHECK(last("erro")["fase"]=="falha"&&last("erro")["id"]==2);
    cmd(1,"INICIAR");CHECK(last("evento")["fase"]=="falhou"&&LittleFS.writeOpens==1);
    cmd(3,"ENVIAR");CHECK(!enviandoGravado);
  }else if(test=="aq_flush_falha"){
    cmd(1,"INICIAR");sample();failFlush=true;cmd(2,"FINALIZAR");
    CHECK(!resumo.fechamentoConfirmado&&!strcmp(resumo.resultado,"FALHOU")&&pins[LED_EXEC]==LOW);
    CHECK(count("codigo","FLASH_FECHAMENTO")==1);cmd(3,"ENVIAR");CHECK(!enviandoGravado);
  }else if(test=="aq_sensor_registro_falho"){
    cmd(1,"INICIAR");sample();celula.ready=false;fakeMillis+=2000;loop();flush();
    CHECK(resumo.fechamentoConfirmado&&!strcmp(resumo.resultado,"FALHOU")&&resumo.amostras==1&&resumo.duracao_ms==2000);
    resumo=ResumoEnsaio();recuperarResumoTeste();CHECK(!strcmp(resumo.resultado,"FALHOU")&&!strcmp(resumo.motivo,"HX711_TIMEOUT"));
    cmd(2,"ENVIAR");CHECK(!enviandoGravado&&count("codigo","FLASH_ENSAIO_NAO_CONCLUIDO")==1);
  }else if(test=="aq_reconectar"){
    cmd(1,"INICIAR");std::string id=resumo.id;sample();Serial.capacidadeTX=0;
    for(int i=0;i<500;i++){fakeMillis+=10;loop();}
    CHECK(ensaioAtivo&&amostrasGravadas==501&&amostrasDescartadasUSB>0&&pins[LED_EXEC]==HIGH);
    Serial.capacidadeTX=64;flush();input(request(0,"IDENTIFICAR",false));CHECK(last("resposta")["maior_id"]==1);
    cmd(2,"STATUS");auto d=last("resposta");CHECK(d["ensaio"]["id"]==id&&d["ensaio"]["amostras"]==501&&d["aquisicao"]["ativa"]==true);
    CHECK(d["ensaio"]["duracao_ms"]==5000&&LittleFS.writeOpens==1);
    cmd(3,"FINALIZAR");CHECK(resumo.fechamentoConfirmado);
  }else if(test=="aq_recuperacao_boot"){
    record();std::string id=resumo.id;resumo=ResumoEnsaio();recuperarResumoTeste();
    CHECK(!strcmp(resumo.resultado,"CONCLUIDO")&&resumo.id==id&&resumo.amostras==3&&resumo.duracao_ms==200&&resumo.fechamentoConfirmado);
    cmd(3,"INICIAR");sample();arquivoEnsaio.close();ensaioAtivo=false;aquisicaoIniciada=false;resumo=ResumoEnsaio();
    recuperarResumoTeste();CHECK(!strcmp(resumo.resultado,"INCOMPLETO")&&!resumo.fechamentoConfirmado);
    cmd(4,"ENVIAR");CHECK(!enviandoGravado);
  }else if(test=="aq_arquivo_antigo"){
    LittleFS.saved=std::make_shared<FileData>();LittleFS.saved->bytes.resize(240,0);LittleFS.files["/ensaio.bin"]=LittleFS.saved;caminhoGravacao[0]=0;
    recuperarResumoTeste();CHECK(!strcmp(resumo.resultado,"INCOMPLETO"));cmd(1,"ENVIAR");CHECK(!enviandoGravado);
    cmd(2,"INICIAR");sample();cmd(3,"FINALIZAR");CHECK(resumo.fechamentoConfirmado);
  }else if(test=="aq_espaco_esgota"){
    LittleFS.capacity=RESERVA_FLASH+sizeof(CabecalhoEnsaio)+2*sizeof(RegistroAmostra)+sizeof(RodapeEnsaio);
    cmd(1,"INICIAR");sample();sample();sample();
    CHECK(!ensaioAtivo&&resumo.amostras==2&&resumo.fechamentoConfirmado&&!strcmp(resumo.resultado,"FALHOU"));
    CHECK(!strcmp(resumo.motivo,"FLASH_SEM_ESPACO")&&count("tipo","amostra")==2);
  }else if(test=="aq_corrupcao_amostra"){
    record();uint32_t bad=999;memcpy(LittleFS.saved->bytes.data()+sizeof(CabecalhoEnsaio)+4,&bad,4);
    cmd(3,"ENVIAR");CHECK(enviandoGravado);loop();flush();CHECK(!enviandoGravado&&LoRa.packets.empty());
    CHECK(count("codigo","FLASH_AMOSTRA_INVALIDA")==1);
  }else if(test=="aq_limites_json"){
    contadorEnsaios=UINT32_MAX-1;payload.fatorCalibracao=1e-300;cmd(4294967290U,"INICIAR_LORA");sample();
    CHECK(LoRa.packets.size()==1&&LoRa.packets[0].size()<256);payload.tempo_ms=UINT32_MAX;payload.indice=UINT32_MAX;
    char json[256];CHECK(montarJson(payload,json,sizeof(json))>0);
    cmd(4294967291U,"STATUS");CHECK(count("codigo","JSON_MEMORIA")==0&&measureJson(last("resposta"))<RESPOSTA_MAX);
    cmd(4294967292U,"FINALIZAR");cmd(4294967293U,"INICIAR");CHECK(!ensaioAtivo&&count("codigo","ENSAIO_ID_ESGOTADO")==1);
  }else if(test=="aq_radio_falha_continua"){
    cmd(1,"INICIAR_LORA");sample();LoRa.txDone=nullptr;
    for(int i=0;i<51;i++){fakeMillis+=100;loop();flush();}
    CHECK(ensaioAtivo&&aquisicaoIniciada&&pins[LED_EXEC]==HIGH&&!transmitirLoRa&&!loraPronto&&amostrasGravadas==52);
    CHECK(count("codigo","LORA_TX_TIMEOUT")==1);cmd(2,"FINALIZAR");CHECK(resumo.fechamentoConfirmado&&!strcmp(resumo.resultado,"CONCLUIDO"));
  }else if(test=="aq_falha_reabertura"){
    cmd(1,"INICIAR");sample();LittleFS.openFail=true;cmd(2,"FINALIZAR");
    CHECK(!resumo.fechamentoConfirmado&&!strcmp(resumo.resultado,"FALHOU")&&count("codigo","FLASH_FECHAMENTO")==1);
    CHECK(last("erro")["id"]==2&&last("erro")["fase"]=="falha");
  }else if(test=="aq_sensor_antigo_inicio"){
    record();auto before=LittleFS.saved->bytes;celula.ready=false;fakeMillis+=2000;
    cmd(3,"INICIAR");CHECK(!ensaioAtivo&&LittleFS.saved->bytes==before&&count("codigo","HX711_INDISPONIVEL")==1);
  }else if(test=="ar_multiplos"){
    auto a=novoEnsaio(1);auto bytes=arquivoDe(a)->bytes;
    auto b=novoEnsaio(3);auto c=novoEnsaio(5);
    CHECK(a!=b&&b!=c&&LittleFS.files.size()==3&&arquivoDe(a)->bytes==bytes);
    listar(7);auto d=last("evento");CHECK(d["codigo"]=="LISTA_ENSAIOS"&&d["ensaios"].size()==3&&d["mais"]==false);
    for(auto id:{a,b,c}){auto info=inspecionarArquivo(id.c_str());CHECK(info.fechado&&info.slots==3&&info.fim.resultado==1);}
  }else if(test=="ar_metadados"){
    payload.fatorCalibracao=-1000;celula.value=1700;tare(1);
    command(2,"INICIAR",R"("nome":"Teste São José","data":"2026-11-15","observacoes":"Linha 1\nLinha 2: \"compressão\"","celula_id":"HZ-2000","perfil_id":"perfil-compressao")");
    std::string id=resumo.id;sample();cmd(3,"FINALIZAR");
    command(4,"CONSULTAR_ENSAIO",idParam(id));auto resposta=last("resposta");auto d=resposta["ensaio"].as<JsonObject>();
    CHECK(d["nome"]=="Teste São José"&&d["data"]=="2026-11-15"&&d["celula_id"]=="HZ-2000"&&d["perfil_id"]=="perfil-compressao");
    CHECK(d["coeficiente"]==-1000&&d["tara_interna"]==1700&&d["unidade"]=="kgf"&&d["condicao"]=="concluido");
    CHECK(d["amostras_confirmadas_no_fim"]==1&&d["duracao_ms"]==0);
    config(5,"123");celula.value=2222;tare(6);command(7,"CONSULTAR_ENSAIO",idParam(id));
    auto out=last("resposta");CHECK(out["ensaio"]["coeficiente"]==-1000&&out["ensaio"]["tara_interna"]==1700);
  }else if(test=="ar_campos_invalidos"){
    for(auto extra:{R"("nome":4)",R"("data":"2026-02-29")",R"("data":"2026-13-01")",R"("data":"15/11/2026")",R"("nome":"a\u0000b")",R"("tara_interna":9)",R"("offset":9)",R"("unidade":"N")",R"("perfil_id":{})"})command(1,"INICIAR",extra);
    command(1,"INICIAR",std::string("\"observacoes\":\"")+std::string(129,'a')+"\"");
    CHECK(!ensaioAtivo&&LittleFS.files.empty()&&maiorId==0);
    command(1,"INICIAR",R"("data":"2024-02-29")");CHECK(ensaioAtivo);
  }else if(test=="ar_dedup_metadados"){
    command(1,"INICIAR",R"("nome":"Primeiro","perfil_id":"P1")");sample();auto id=std::string(resumo.id);
    command(1,"INICIAR",R"("perfil_id":"P1","nome":"Primeiro")");CHECK(LittleFS.files.size()==1&&amostrasGravadas==1);
    command(1,"INICIAR",R"("nome":"Segundo","perfil_id":"P1")");CHECK(count("codigo","ID_CONFLITO")==1&&LittleFS.files.size()==1);
    cmd(2,"FINALIZAR");command(1,"INICIAR",R"("nome":"Primeiro","perfil_id":"P1")");CHECK(!ensaioAtivo&&resumo.id==id);
  }else if(test=="ar_paginacao"){
    std::vector<std::string> ids;
    for(int i=0;i<9;i++)ids.push_back(novoEnsaio(1+i*2));
    listar(20,2);auto page=last("evento");CHECK(page["ensaios"].size()==2&&page["mais"]==true);
    uint32_t rev=page["catalogo_revisao"];std::vector<std::string> vistos;
    uint32_t n=21;
    while(true){
      for(auto x:page["ensaios"].as<JsonArray>())vistos.push_back(x["ensaio_id"].as<std::string>());
      if(!page["mais"].as<bool>())break;
      auto apos=page["proximo"].as<std::string>();listar(n++,2,apos,rev);page=last("evento");
    }
    std::sort(ids.begin(),ids.end());CHECK(vistos==ids);
    cmd(30,"STATUS");CHECK(last("resposta")["catalogo"]["ocupado"]==false);
  }else if(test=="ar_pagina_revisao"){
    auto id=novoEnsaio(1);listar(3,1);uint32_t rev=last("evento")["catalogo_revisao"];
    novoEnsaio(4);listar(6,1,id,rev);CHECK(last("erro")["codigo"]=="CATALOGO_ALTERADO");
    command(7,"LISTAR_ENSAIOS",R"("limite":2)");CHECK(operacaoCatalogo.tipo==OperacaoCatalogo::LISTAR);
    command(8,"EXCLUIR_ENSAIOS",idsParam({id}));drainCatalog();CHECK(last("evento")["codigo"]=="CATALOGO_ALTERADO");
  }else if(test=="ar_listar_adquirindo"){
    auto old=novoEnsaio(1);(void)old;cmd(3,"INICIAR");std::string ativo=resumo.id;
    command(4,"LISTAR_ENSAIOS",R"("limite":4)");
    uint32_t antes=amostrasGravadas;for(int i=0;i<15;i++){fakeMillis+=10;loop();flush();}
    CHECK(ensaioAtivo&&amostrasGravadas>antes&&count("codigo","LISTA_ENSAIOS")==1);
    bool found=false;for(auto&d:messages())if(d["codigo"]=="LISTA_ENSAIOS")for(auto e:d["ensaios"].as<JsonArray>())if(e["ensaio_id"]==ativo){CHECK(e["em_uso"]==true&&e["condicao"]=="em_andamento");found=true;}
    CHECK(found);
  }else if(test=="ar_blocos"){
    cmd(1,"INICIAR");std::string id=resumo.id;
    for(int i=0;i<9;i++){sample();}cmd(2,"FINALIZAR");
    std::vector<uint32_t> indices;
    for(uint32_t start=1,requestId=3;start<=9;requestId++){
      command(requestId,"LER_AMOSTRAS",idParam(id)+",\"inicio\":"+std::to_string(start)+",\"limite\":4");auto d=last("resposta");
      CHECK(d["ensaio_id"]==id&&d["inicio"]==start&&d["erros"]==0&&d["amostras"].size()<=4);
      CHECK(d["bloco_id"].as<std::string>()==id+":"+std::to_string(start)+":4");
      for(auto x:d["amostras"].as<JsonArray>()){indices.push_back(x["indice"]);CHECK(x["valida"]==true&&x["empuxo_kgf"]==1500);}
      start=d["proximo"];
    }
    CHECK(indices==std::vector<uint32_t>({1,2,3,4,5,6,7,8,9}));
    command(6,"LER_AMOSTRAS",idParam(id)+",\"inicio\":10");CHECK(last("resposta")["amostras"].size()==0&&last("resposta")["fim"]==true);
    command(7,"LER_AMOSTRAS",idParam(id)+",\"inicio\":11");CHECK(last("erro")["codigo"]=="BLOCO_FORA_LIMITE");
  }else if(test=="ar_blocos_dedup"){
    auto id=novoEnsaio(1);command(3,"LER_AMOSTRAS",idParam(id)+",\"inicio\":1,\"limite\":2");auto before=last("resposta");
    command(3,"LER_AMOSTRAS",idParam(id)+",\"limite\":2,\"inicio\":1");auto after=last("resposta");CHECK(before==after);
    command(3,"LER_AMOSTRAS",idParam(id)+",\"inicio\":2,\"limite\":2");CHECK(last("erro")["codigo"]=="ID_CONFLITO");
  }else if(test=="ar_recuperar_incompleto"){
    cmd(1,"INICIAR");auto id=std::string(resumo.id);sample();sample();failWriteCall=writeCalls+1;sample();
    auto info=inspecionarArquivo(id.c_str());CHECK(!info.fechado&&info.slots==2&&info.cabecalhoOk);
    command(2,"CONSULTAR_ENSAIO",idParam(id));auto d=last("resposta");CHECK(d["ensaio"]["condicao"]=="incompleto"&&d["ensaio"]["duracao_ms"].isNull()&&d["ensaio"]["duracao_observada_ms"]==100);
    command(3,"LER_AMOSTRAS",idParam(id));d=last("resposta");CHECK(d["amostras"].size()==2&&d["erros"]==0&&d["condicao"]=="incompleto");
  }else if(test=="ar_crc_amostra"){
    auto id=novoEnsaio(1);auto &bytes=arquivoDe(id)->bytes;bytes[sizeof(CabecalhoEnsaio)+sizeof(RegistroAmostra)+8]^=1;
    command(3,"LER_AMOSTRAS",idParam(id));auto d=last("resposta");CHECK(d["erros"]==1&&d["amostras"].size()==3);
    CHECK(d["amostras"][0]["valida"]==true&&d["amostras"][1]["valida"]==false&&d["amostras"][2]["valida"]==true);
    CHECK(d["amostras"][1]["empuxo_kgf"].isNull());
  }else if(test=="ar_crc_cabecalho"){
    auto id=novoEnsaio(1);arquivoDe(id)->bytes[56]^=1;
    command(3,"CONSULTAR_ENSAIO",idParam(id));auto d=last("resposta");CHECK(d["ensaio"]["condicao"]=="incompleto"&&d["ensaio"]["leitura_suportada"]==false);
    command(4,"LER_AMOSTRAS",idParam(id));CHECK(last("erro")["codigo"]=="FORMATO_NAO_SUPORTADO");
  }else if(test=="ar_crc_rodape"){
    auto id=novoEnsaio(1);auto &bytes=arquivoDe(id)->bytes;bytes[bytes.size()-1]^=1;
    auto info=inspecionarArquivo(id.c_str());CHECK(!info.fechado&&info.slots==3&&info.cabecalhoOk);
    command(3,"LER_AMOSTRAS",idParam(id));CHECK(last("resposta")["erros"]==0&&last("resposta")["amostras"].size()==3);
  }else if(test=="ar_excluir_selecionados"){
    auto a=novoEnsaio(1),b=novoEnsaio(3),c=novoEnsaio(5);
    command(7,"EXCLUIR_ENSAIOS",idsParam({a,c,"INEXISTENTE"}));auto d=last("resposta");
    CHECK(d["resultados"].size()==3&&d["resultados"][0]["codigo"]=="EXCLUIDO"&&d["resultados"][1]["codigo"]=="EXCLUIDO"&&d["resultados"][2]["codigo"]=="NAO_ENCONTRADO");
    CHECK(LittleFS.files.size()==1&&arquivoDe(b));
    command(7,"EXCLUIR_ENSAIOS",idsParam({a,c,"INEXISTENTE"}));CHECK(last("resposta")==d&&LittleFS.files.size()==1);
    command(7,"EXCLUIR_ENSAIOS",idsParam({b}));CHECK(last("erro")["codigo"]=="ID_CONFLITO"&&arquivoDe(b));
  }else if(test=="ar_protecao_gravacao"){
    auto old=novoEnsaio(1);cmd(3,"INICIAR");auto id=std::string(resumo.id);
    command(4,"EXCLUIR_ENSAIOS",idsParam({old,id}));auto d=last("resposta");CHECK(d["resultados"][0]["ok"]==true&&d["resultados"][1]["codigo"]=="ENSAIO_PROTEGIDO");
    CHECK(ensaioAtivo&&arquivoDe(id));sample();command(5,"LER_AMOSTRAS",idParam(id));CHECK(last("erro")["codigo"]=="ENSAIO_PROTEGIDO");
    command(6,"CONSULTAR_ENSAIO",idParam(id));CHECK(last("resposta")["ensaio"]["condicao"]=="em_andamento");
    cmd(7,"FINALIZAR");CHECK(resumo.fechamentoConfirmado);
  }else if(test=="ar_protecao_transmissao"){
    auto a=novoEnsaio(1),b=novoEnsaio(3);command(5,"ENVIAR",idParam(a));CHECK(enviandoGravado&&idTransmissao==a);
    command(6,"EXCLUIR_ENSAIOS",idsParam({a,b}));auto d=last("resposta");CHECK(d["resultados"][0]["codigo"]=="ENSAIO_PROTEGIDO"&&d["resultados"][1]["ok"]==true);
    command(7,"LER_AMOSTRAS",idParam(a));CHECK(last("erro")["codigo"]=="ENSAIO_PROTEGIDO");
    for(int i=0;i<20;i++){fakeMillis+=100;loop();flush();}CHECK(!enviandoGravado&&LoRa.packets.size()==3);
    for(auto text:LoRa.packets){JsonDocument p;deserializeJson(p,text);CHECK(p["ensaio_id"]==a);}
  }else if(test=="ar_excluir_todos"){
    for(uint32_t i=0;i<11;i++)novoEnsaio(1+i*2);
    LittleFS.files["/outro.txt"]=std::make_shared<FileData>();
    command(23,"EXCLUIR_TODOS",R"("confirmacao":"EXCLUIR_TODOS")");CHECK(last("resposta")["fase"]=="aceito");
    drainCatalog();auto d=last("evento");CHECK(d["codigo"]=="EXCLUSAO_TOTAL_CONCLUIDA"&&d["excluidos"]==11&&d["todos_excluidos"]==true);
    CHECK(count("codigo","EXCLUSAO_RESULTADO")==11&&LittleFS.files.size()==1&&LittleFS.files.count("/outro.txt"));
    command(23,"EXCLUIR_TODOS",R"("confirmacao":"EXCLUIR_TODOS")");CHECK(last("evento")["excluidos"]==11&&count("codigo","EXCLUSAO_RESULTADO")==11);
  }else if(test=="ar_excluir_todos_protegido"){
    auto old=novoEnsaio(1);(void)old;cmd(3,"INICIAR");auto id=std::string(resumo.id);sample();
    command(4,"EXCLUIR_TODOS",R"("confirmacao":"EXCLUIR_TODOS")");cmd(5,"FINALIZAR");
    cmd(6,"INICIAR");CHECK(!ensaioAtivo&&last("erro")["codigo"]=="CATALOGO_OCUPADO");
    drainCatalog();auto d=last("evento");CHECK(d["excluidos"]==1&&d["protegidos"]==1&&d["todos_excluidos"]==false&&arquivoDe(id));
  }else if(test=="ar_excluir_todos_sem_confirmar"){
    auto id=novoEnsaio(1);cmd(3,"EXCLUIR_TODOS");CHECK(last("erro")["codigo"]=="PARAMETRO_INVALIDO"&&arquivoDe(id));
    command(3,"EXCLUIR_TODOS",R"("confirmacao":"sim")");CHECK(operacaoCatalogo.tipo==OperacaoCatalogo::NENHUMA&&arquivoDe(id));
  }else if(test=="ar_exclusao_falha"){
    auto a=novoEnsaio(1),b=novoEnsaio(3);char path[64];caminhoEnsaio(a.c_str(),path,sizeof(path));LittleFS.removeFailPath=path;
    command(5,"EXCLUIR_ENSAIOS",idsParam({a,b}));auto d=last("resposta");CHECK(d["resultados"][0]["codigo"]=="FLASH_EXCLUSAO"&&d["resultados"][1]["ok"]==true&&arquivoDe(a));
    command(6,"EXCLUIR_TODOS",R"("confirmacao":"EXCLUIR_TODOS")");drainCatalog();d=last("evento");CHECK(d["falhas"]==1&&d["todos_excluidos"]==false&&arquivoDe(a));
  }else if(test=="ar_espaco"){
    cmd(1,"ESPACO");auto before=last("resposta");auto id=novoEnsaio(2);(void)id;
    cmd(4,"ESPACO");auto after=last("resposta");CHECK(after["usado_bytes"].as<size_t>()>before["usado_bytes"].as<size_t>());
    CHECK(after["total_bytes"].as<size_t>()==after["usado_bytes"].as<size_t>()+after["livre_bytes"].as<size_t>());
    CHECK(after["reserva_bytes"]==RESERVA_FLASH&&after["backend"]=="LittleFS_interno");
  }else if(test=="ar_sem_espaco_preserva"){
    auto a=novoEnsaio(1),b=novoEnsaio(3);auto bytesA=arquivoDe(a)->bytes,bytesB=arquivoDe(b)->bytes;
    LittleFS.capacity=LittleFS.usedBytes()+RESERVA_FLASH;cmd(5,"INICIAR");
    CHECK(!ensaioAtivo&&last("erro")["codigo"]=="FLASH_SEM_ESPACO"&&arquivoDe(a)->bytes==bytesA&&arquivoDe(b)->bytes==bytesB);
  }else if(test=="ar_falha_novo_preserva"){
    auto a=novoEnsaio(1),b=novoEnsaio(3);auto bytesA=arquivoDe(a)->bytes,bytesB=arquivoDe(b)->bytes;
    failWriteCall=writeCalls+1;cmd(5,"INICIAR");CHECK(!ensaioAtivo&&arquivoDe(a)->bytes==bytesA&&arquivoDe(b)->bytes==bytesB);
    CHECK(LittleFS.files.size()==3);listar(6);CHECK(last("evento")["ensaios"].size()==3);
  }else if(test=="ar_montagem_sem_formatar"){
    auto a=novoEnsaio(1);auto before=arquivoDe(a)->bytes;LittleFS.ready=false;setup();flush();
    CHECK(!memoriaPronta&&!LittleFS.formatFlag&&arquivoDe(a)->bytes==before);
    cmd(3,"ESPACO");CHECK(last("erro")["codigo"]=="FLASH_INDISPONIVEL");
  }else if(test=="ar_legado"){
    LittleFS.files["/ensaio.bin"]=std::make_shared<FileData>();auto legacy=LittleFS.files["/ensaio.bin"];legacy->bytes.resize(120);
    uint32_t prefix[]={0x42454E32,2};memcpy(legacy->bytes.data(),prefix,sizeof(prefix));auto before=legacy->bytes;
    auto id=novoEnsaio(1);(void)id;CHECK(legacy->bytes==before);listar(3);CHECK(last("evento")["ensaios"].size()==2);
    command(4,"CONSULTAR_ENSAIO",idParam(ID_LEGADO));auto d=last("resposta");CHECK(d["ensaio"]["condicao"]=="legado"&&d["ensaio"]["formato"]==2&&d["ensaio"]["leitura_suportada"]==false);
    command(5,"LER_AMOSTRAS",idParam(ID_LEGADO));CHECK(last("erro")["codigo"]=="FORMATO_NAO_SUPORTADO");
    command(6,"ENVIAR",idParam(ID_LEGADO));CHECK(last("erro")["codigo"]=="FORMATO_NAO_SUPORTADO"&&legacy->bytes==before);
    command(7,"EXCLUIR_ENSAIOS",idsParam({ID_LEGADO}));CHECK(!LittleFS.files.count("/ensaio.bin"));
  }else if(test=="ar_formato_futuro"){
    auto id=novoEnsaio(1);uint32_t futuro=99;memcpy(arquivoDe(id)->bytes.data()+4,&futuro,4);
    command(3,"CONSULTAR_ENSAIO",idParam(id));CHECK(last("resposta")["ensaio"]["condicao"]=="nao_suportado");
    command(4,"LER_AMOSTRAS",idParam(id));CHECK(last("erro")["codigo"]=="FORMATO_NAO_SUPORTADO");
  }else if(test=="ar_ids_inseguros"){
    auto id=novoEnsaio(1);auto before=arquivoDe(id)->bytes;
    for(auto x:{"../ensaio","/ensaio.bin","a/b","a\\b","", "12345678901234567890123456789012"}){
      command(3,"CONSULTAR_ENSAIO",idParam(x));command(3,"EXCLUIR_ENSAIOS",idsParam({x}));
    }
    command(3,"EXCLUIR_ENSAIOS",idsParam({id,id}));command(3,"EXCLUIR_ENSAIOS",R"("ensaio_ids":[])");
    command(3,"EXCLUIR_ENSAIOS",idsParam({id,"a","b","c","d"}));CHECK(LittleFS.files.size()==1&&arquivoDe(id)->bytes==before&&maiorId==2);
  }else if(test=="ar_catalogo_backpressure"){
    for(int i=0;i<6;i++)novoEnsaio(1+i*2);
    command(13,"EXCLUIR_TODOS",R"("confirmacao":"EXCLUIR_TODOS")");Serial.capacidadeTX=0;
    for(int i=0;i<200;i++){loop();}
    CHECK(mensagensDescartadasUSB==0);
    Serial.capacidadeTX=64;drainCatalog();flush();CHECK(last("evento")["excluidos"]==6&&count("codigo","EXCLUSAO_RESULTADO")==6);
  }else if(test=="ar_metadados_maximos"){
    JsonDocument p;p["nome"]=std::string(48,'"');p["data"]="2026-11-15";p["observacoes"]=std::string(128,'A');
    p["celula_id"]=std::string(32,'C');p["perfil_id"]=std::string(32,'P');
    std::string params;serializeJson(p,params);params=params.substr(1,params.size()-2);
    command(1,"INICIAR",params);CHECK(ensaioAtivo);std::string id=resumo.id;sample();cmd(2,"FINALIZAR");
    command(3,"CONSULTAR_ENSAIO",idParam(id));CHECK(count("codigo","JSON_MEMORIA")==0&&last("resposta")["ensaio"]["nome"].as<JsonString>().size()==48);
  }else if(test=="ar_colisao_ids"){
    auto a=novoEnsaio(1),b=novoEnsaio(3);auto before=arquivoDe(a)->bytes;contadorEnsaios=0;
    cmd(5,"INICIAR");CHECK(ensaioAtivo&&std::string(resumo.id)!=a&&std::string(resumo.id)!=b&&arquivoDe(a)->bytes==before&&LittleFS.files.size()==3);
  }else if(test=="ar_reboot_gravar"){
    auto a=novoEnsaio(1),b=novoEnsaio(3);(void)a;(void)b;
    command(5,"INICIAR",R"("nome":"Interrompido","data":"2026-11-15","celula_id":"C20","perfil_id":"P20")");sample();sample();
    RegistroAmostra parcial={};arquivoEnsaio.write((uint8_t*)&parcial,17);arquivoEnsaio.flush();
    salvarDisco();CHECK(LittleFS.files.size()==3); // termina processo sem FINALIZAR
  }else if(test=="ar_reboot_ler"){
    CHECK(LittleFS.files.size()==3&&!ensaioAtivo&&!LittleFS.formatFlag);
    listar(1);auto page=last("evento");CHECK(page["ensaios"].size()==3);
    int concluidos=0,incompletos=0;std::vector<std::vector<uint8_t>> originais;
    for(auto item:page["ensaios"].as<JsonArray>()){
      auto id=item["ensaio_id"].as<std::string>();auto info=inspecionarArquivo(id.c_str());originais.push_back(arquivoDe(id)->bytes);
      if(info.fechado&&info.fim.resultado==1){concluidos++;CHECK(info.slots==3);}
      else {incompletos++;CHECK(info.slots==2&&info.cabecalhoOk&&!strcmp(info.cab.nome,"Interrompido")&&info.cab.taraInterna==1000);command(2,"LER_AMOSTRAS",idParam(id));CHECK(last("resposta")["amostras"].size()==2&&last("resposta")["erros"]==0);}
    }
    CHECK(concluidos==2&&incompletos==1);
    CHECK(!calibracaoConfigurada);config(3,"1");tare(4);cmd(5,"INICIAR");CHECK(ensaioAtivo&&LittleFS.files.size()==4);size_t n=0;
    for(auto item:page["ensaios"].as<JsonArray>()){CHECK(arquivoDe(item["ensaio_id"].as<std::string>())->bytes==originais[n++]);}
  }else if(test=="ar_falha_listagem"){
    auto id=novoEnsaio(1);command(3,"LISTAR_ENSAIOS",R"("limite":4)");failDirectory=true;drainCatalog();
    CHECK(last("evento")["codigo"]=="FLASH_LISTAGEM"&&last("evento")["fase"]=="falhou"&&arquivoDe(id));
    failDirectory=false;command(4,"EXCLUIR_TODOS",R"("confirmacao":"EXCLUIR_TODOS")");failDirectory=true;drainCatalog();
    CHECK(last("evento")["fase"]=="falhou"&&last("evento")["todos_excluidos"]==false&&arquivoDe(id));
  }else if(test=="ar_falha_abrir_detalhes"){
    auto id=novoEnsaio(1);char path[64];caminhoEnsaio(id.c_str(),path,sizeof(path));LittleFS.openFailPath=path;
    listar(3);auto d=last("evento");CHECK(d["ensaios"].size()==1&&d["ensaios"][0]["condicao"]=="indisponivel");
    command(4,"CONSULTAR_ENSAIO",idParam(id));CHECK(last("erro")["codigo"]=="ENSAIO_INDISPONIVEL");
  }else if(test=="ar_diretorio_falha"){
    auto id=novoEnsaio(1);LittleFS.dirExists=false;LittleFS.mkdirFail=true;setup();flush();
    CHECK(!memoriaPronta&&!LittleFS.formatFlag&&arquivoDe(id));
    cmd(3,"LISTAR_ENSAIOS");CHECK(last("erro")["codigo"]=="FLASH_INDISPONIVEL");
  }else if(test=="ar_limites_parametros"){
    auto id=novoEnsaio(1);
    for(auto extra:{R"("limite":0)",R"("limite":5)",R"("limite":1.0)",R"("apos":"ABC")",R"("revisao":0)"})command(3,"LISTAR_ENSAIOS",extra);
    for(auto extra:{",\"inicio\":0",",\"inicio\":-1",",\"limite\":5",",\"inicio\":4294967296"})command(3,"LER_AMOSTRAS",idParam(id)+extra);
    CHECK(maiorId==2&&count("tipo","erro")==9&&arquivoDe(id));
  }else if(test=="ar_limites_respostas"){
    JsonDocument p;p["nome"]=std::string(48,'\"');p["observacoes"]=std::string(128,'\"');
    p["celula_id"]=std::string(32,'\"');p["perfil_id"]=std::string(32,'\"');p["data"]="2026-11-15";
    // O envelope RX continua limitado a 512 bytes. Exercita limites de saida com cabecalho valido controlado.
    auto id=novoEnsaio(1);auto file=arquivoDe(id);CabecalhoEnsaio cab={};memcpy(&cab,file->bytes.data(),sizeof(cab));
    strcpy(cab.nome,p["nome"]);strcpy(cab.observacoes,p["observacoes"]);strcpy(cab.celulaId,p["celula_id"]);strcpy(cab.perfilId,p["perfil_id"]);
    cab.crc=crc32(&cab,offsetof(CabecalhoEnsaio,crc));memcpy(file->bytes.data(),&cab,sizeof(cab));
    command(3,"CONSULTAR_ENSAIO",idParam(id));CHECK(count("codigo","JSON_MEMORIA")==0&&measureJson(last("resposta"))<RESPOSTA_MAX);
  }else if(test=="ar_protecao_ultimo_pacote"){
    auto id=novoEnsaio(1);command(3,"ENVIAR",idParam(id));loop();flush();CHECK(loraTransmitindo);
    cmd(4,"CANCELAR");CHECK(!enviandoGravado&&loraTransmitindo);
    command(5,"EXCLUIR_ENSAIOS",idsParam({id}));CHECK(last("resposta")["resultados"][0]["codigo"]=="ENSAIO_PROTEGIDO");
    fakeMillis+=100;loop();command(6,"EXCLUIR_ENSAIOS",idsParam({id}));CHECK(last("resposta")["resultados"][0]["codigo"]=="EXCLUIDO");
  }else if(test=="ar_protecao_pacote_ao_vivo"){
    cmd(1,"INICIAR_LORA");auto id=std::string(resumo.id);sample();CHECK(loraTransmitindo);cmd(2,"FINALIZAR");
    command(3,"EXCLUIR_TODOS",R"("confirmacao":"EXCLUIR_TODOS")");fakeMillis+=100;LoRa.tick();drainCatalog();
    CHECK(last("evento")["protegidos"]==1&&arquivoDe(id));
  }else if(test=="ar_nomes_paginados"){
    std::map<std::string,std::string> nomes;
    for(uint32_t i=0;i<3;i++){
      auto nome=std::string("Ensaio ")+std::to_string(i);
      command(1+i*2,"INICIAR",std::string("\"nome\":\"")+nome+"\"");
      nomes[resumo.id]=nome;sample();cmd(2+i*2,"FINALIZAR");
    }
    listar(7);auto d=last("evento");CHECK(d["ensaios"].size()==3);
    for(auto item:d["ensaios"].as<JsonArray>())CHECK(item["nome"].as<std::string>()==nomes[item["ensaio_id"].as<std::string>()]);
  }else CHECK(false);
  messages();std::cout<<Serial.output;std::cerr<<"PASS "<<test<<'\n';
}
