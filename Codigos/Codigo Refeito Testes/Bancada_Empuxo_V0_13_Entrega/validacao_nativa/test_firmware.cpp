#include <iostream>
#include <sstream>
#include <cstdlib>
#define loop loopFirmware
#include "../Bancada_Empuxo_V0_13/Bancada_Empuxo_V0_13.ino"
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
void sample(){fakeMillis+=100;celula.value=2500;loop();flush();}
void record(){cmd(1,"INICIAR");sample();sample();sample();cmd(2,"FINALIZAR");}

int main(int argc,char**argv){
  CHECK(argc==2);std::string test=argv[1];
  if(test=="sem_hx")celula.ready=false;
  if(test=="sem_lora")LoRa.ready=false;
  if(test=="sem_flash")LittleFS.ready=false;
  setup();
  if(test!="boot_responsivo"&&test!="sem_hx")for(int i=0;i<10;i++)loop();
  flush();Serial.output.clear();

  if(test=="identidade"){
    input(request(0,"IDENTIFICAR",false));auto d=last("resposta");
    CHECK(d["firmware"]=="0.13"&&d["id"]==0&&d["baud"]==115200);
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
    cmd(20,"FINALIZAR");Serial.output.clear();cmd(1,"INICIAR");CHECK(last("evento")["fase"]=="concluido");
  }else if(test=="dedup_alternar"){
    cmd(1,"INICIAR");cmd(2,"LORA_ALTERNAR");CHECK(transmitirLoRa);cmd(2,"LORA_ALTERNAR");CHECK(transmitirLoRa);
  }else if(test=="dedup_erro"){
    cmd(1,"LORA_LIGAR");cmd(2,"INICIAR");cmd(1,"LORA_LIGAR");CHECK(!transmitirLoRa&&count("codigo","ENSAIO_INATIVO")==2);
  }else if(test=="status_snapshot"){
    cmd(1,"STATUS");cmd(2,"INICIAR");cmd(1,"STATUS");CHECK(last("resposta")["estado"]=="OCIOSO");
    cmd(3,"STATUS");CHECK(last("resposta")["estado"]=="ADQUIRINDO");
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
    fakeMillis=3000;loop();flush();CHECK(!taraPronta&&count("codigo","HX711_TARA_TIMEOUT")==1);
  }else if(test=="boot_responsivo"){
    CHECK(taraInicialAtiva&&fakeMillis==0);cmd(1,"STATUS");CHECK(last("resposta")["sensor"]["tara_em_andamento"]==true);
    cmd(2,"INICIAR");CHECK(!ensaioAtivo);for(int i=0;i<10;i++)loop();cmd(3,"INICIAR");CHECK(ensaioAtivo);
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
    CHECK(d["empuxo_kgf"]==1.5&&d["ensaio_id"]==1&&d["indice"]==1);
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
    CHECK(modes.size()==2&&LittleFS.formatFlag);
  }else CHECK(false);
  messages();std::cout<<Serial.output;std::cerr<<"PASS "<<test<<'\n';
}
