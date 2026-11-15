#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#define loop loopFirmware
#include "../Bancada_Empuxo_V0_12/Bancada_Empuxo_V0_12.ino"
#undef loop
void loop(){LoRa.tick();loopFirmware();}

#define CHECK(x) do{if(!(x)){std::cerr<<"Falha linha "<<__LINE__<<": "<<#x<<'\n';exit(1);}}while(0)

void command(const std::string& s){
  Serial.feed(s);
  for(int i=0;Serial.available()&&i<2000;i++)ReceberComando();
  CHECK(!Serial.available());
}
std::vector<JsonDocument> messages(){
  std::vector<JsonDocument> out;
  std::istringstream lines(Serial.output);
  std::string line;
  while(std::getline(lines,line)){
    JsonDocument d;
    CHECK(!deserializeJson(d,line));
    CHECK(d["protocolo"].as<int>()==1);
    CHECK(d["tipo"].is<const char*>());
    out.push_back(std::move(d));
  }
  return out;
}
int count(const char* key,const char* value){
  int n=0;
  for(auto& d:messages())if(d[key].is<const char*>()&&!strcmp(d[key].as<const char*>(),value))n++;
  return n;
}
JsonDocument last(const char* type){
  auto all=messages();
  for(auto i=all.rbegin();i!=all.rend();++i)if((*i)["tipo"]==type)return *i;
  CHECK(false);return JsonDocument();
}
void storeSamples(int n){
  command("i\n");CHECK(ensaioAtivo);
  for(int i=1;i<=n;i++){fakeMillis+=100;celula.value=1000+i*100;loop();}
  command("f\n");CHECK(!ensaioAtivo);CHECK(amostrasGravadas==(uint32_t)n);
}

int main(int argc,char** argv){
  CHECK(argc==2);
  std::string test=argv[1];
  if(test=="boot_sem_hx")celula.ready=false;
  if(test=="tara_incompleta")celula.remaining=5;
  if(test=="sem_lora")LoRa.ready=false;
  if(test=="sem_flash")LittleFS.ready=false;
  setup();

  if(test=="configuracao"){
    CHECK(Serial.baud==115200);CHECK(celula.dt==33&&celula.sck==32);
    CHECK(celula.offset==1000&&taraPronta);
    CHECK(SPI.args==std::vector<int>({18,19,23,21}));
    CHECK(LoRa.pins==std::vector<int>({21,14,26}));
    CHECK(LoRa.spiHz==1000000&&LoRa.frequency==915E6&&LoRa.sync==145&&LoRa.power==2);
    CHECK(modes.size()==2&&modes[4]==OUTPUT&&modes[2]==OUTPUT);
    CHECK(pins[4]==LOW&&pins[2]==LOW&&LittleFS.formatFlag);
    command("status\r\nAJUDA\n");
    CHECK(count("tipo","status")==2);CHECK(count("tipo","ajuda")==1);
    auto d=last("status");CHECK(d["firmware"]=="0.12"&&d["unidade"]=="kgf");
    CHECK(d["hx711_respondendo"]==true&&d["motor_implementado"]==false);
  } else if(test=="enquadramento"){
    command("INICIAR");CHECK(!ensaioAtivo);
    command("\n");CHECK(ensaioAtivo&&LittleFS.writeOpens==1);
    command("falar\nINICIAR_MOTOR\nif\n{\"cmd\":\"i\"}\n");
    CHECK(ensaioAtivo&&LittleFS.writeOpens==1);
    CHECK(count("codigo","COMANDO_DESCONHECIDO")==4);
    command(" \tFinalizar\t \r\n");CHECK(!ensaioAtivo);
    command("f\nf\nc\n");CHECK(!ensaioAtivo&&!enviandoGravado);
  } else if(test=="limites"){
    command(std::string(63,'x')+"\n");CHECK(count("codigo","COMANDO_DESCONHECIDO")==1);
    command(std::string(64,'x')+"i\n");CHECK(count("codigo","COMANDO_LONGO")==1);
    CHECK(!ensaioAtivo);
    command(std::string("i\0i\n",4));CHECK(!ensaioAtivo);
    command(std::string("\xc3\xa7i\n",4));CHECK(!ensaioAtivo);
    CHECK(count("codigo","COMANDO_CARACTERE")==2);
    command(" \n\r\n s\r\n");CHECK(count("tipo","status")==2);
    command("i\n");CHECK(ensaioAtivo);
  } else if(test=="medicao"){
    payload.fatorCalibracao=1000.0;
    command("i\n");celula.value=2500;fakeMillis+=100;loop();
    auto d=last("amostra");CHECK(d["empuxo_kgf"].as<double>()==1.5);
    CHECK(d["tempo_ms"].as<int>()==100&&d["indice"].as<int>()==1);
    CHECK(pins[4]==HIGH&&pins[2]==LOW);
    CHECK(LittleFS.saved->bytes.size()==sizeof(Payload));
    Payload p;memcpy(&p,LittleFS.saved->bytes.data(),sizeof(p));
    CHECK(p.empuxo==1.5&&p.fatorCalibracao==1000.0);
    command("f\ni\n");payload.fatorCalibracao=-1000;loop();
    CHECK(last("amostra")["empuxo_kgf"].as<double>()==-1.5);
  } else if(test=="boot_sem_hx"||test=="tara_incompleta"){
    CHECK(fakeMillis==3000&&!taraPronta);
    CHECK(count("codigo","HX711_TARA_TIMEOUT")==1);
    command("s\ni\n");CHECK(!ensaioAtivo&&LittleFS.writeOpens==0);
    CHECK(count("codigo","HX711_INDISPONIVEL")==1);
    celula.ready=true;celula.remaining=-1;loop();command("i\n");
    CHECK(!ensaioAtivo&&!taraPronta);
  } else if(test=="timeout_hx"){
    command("i\n");loop();celula.ready=false;
    fakeMillis+=2000;loop();loop();
    CHECK(!ensaioAtivo&&!taraPronta&&!arquivoEnsaio&&pins[4]==LOW);
    CHECK(count("codigo","HX711_TIMEOUT")==1);
    celula.ready=true;loop();command("i\n");CHECK(!ensaioAtivo);
  } else if(test=="sem_lora"){
    command("a\n");CHECK(!ensaioAtivo&&LittleFS.writeOpens==0);
    command("i\n");CHECK(ensaioAtivo);
    command("l\n");CHECK(!transmitirLoRa);
    CHECK(count("codigo","LORA_INDISPONIVEL")==2);
  } else if(test=="sem_flash"){
    command("i\n");CHECK(!ensaioAtivo&&LittleFS.writeOpens==0);
    CHECK(count("codigo","FLASH_INICIALIZACAO")==1);
    CHECK(count("codigo","FLASH_INDISPONIVEL")==1);
  } else if(test=="atomicidade"){
    command("i\n");loop();auto start=inicioEnsaio;
    command("a\n");CHECK(ensaioAtivo&&!transmitirLoRa&&inicioEnsaio==start);
    CHECK(LittleFS.writeOpens==1&&amostrasGravadas==1);
    command("LORA_LIGAR\nLORA_LIGAR\n");CHECK(transmitirLoRa);
    command("LORA_DESLIGAR\nLORA_DESLIGAR\n");CHECK(!transmitirLoRa);
    command("e\n");CHECK(ensaioAtivo&&!enviandoGravado);
    command("f\ne\ni\n");CHECK(enviandoGravado&&!ensaioAtivo);
    command("c\n");CHECK(!enviandoGravado&&!arquivoEnsaio);
  } else if(test=="falha_abertura"){
    LittleFS.openFail=true;command("i\n");CHECK(!ensaioAtivo);
    CHECK(count("codigo","FLASH_ABERTURA")==1);
  } else if(test=="falha_gravacao"){
    command("i\n");failWrite=true;loop();
    CHECK(!ensaioAtivo&&amostrasGravadas==0&&count("tipo","amostra")==0);
    CHECK(count("codigo","FLASH_GRAVACAO")==1);
    command("e\n");CHECK(!enviandoGravado);CHECK(count("codigo","FLASH_FORMATO")==1);
  } else if(test=="sem_ensaio"){
    command("e\n");CHECK(!enviandoGravado);CHECK(count("codigo","SEM_ENSAIO")==1);
  } else if(test=="retransmissao"){
    storeSamples(3);LoRa.txDuration=400;command("e\n");
    for(int i=0;i<80&&enviandoGravado;i++){fakeMillis+=50;loop();}
    CHECK(!enviandoGravado&&amostrasEnviadas==3&&LoRa.packets.size()==3);
    for(int i=0;i<3;i++){
      JsonDocument d;CHECK(!deserializeJson(d,LoRa.packets[i]));
      CHECK(d["empuxo"].as<int>()==(i+1)*100);
      CHECK(d.size()==3&&d["fator_calibracao"]==1.0);
    }
    CHECK(count("codigo","ENVIO_CONCLUIDO")==1);
  } else if(test=="falha_leitura"){
    storeSamples(1);command("e\n");failRead=true;loop();
    CHECK(!enviandoGravado&&count("codigo","FLASH_LEITURA")==1);
  } else if(test=="falha_seek"){
    storeSamples(2);LoRa.txDuration=500;command("e\n");loop();
    failSeek=true;fakeMillis+=250;loop();
    CHECK(!enviandoGravado&&count("codigo","FLASH_POSICAO")==1);
  } else if(test=="falha_lora"){
    command("a\n");LoRa.writeFail=true;loop();
    CHECK(ensaioAtivo&&!transmitirLoRa&&count("codigo","LORA_ENVIO")==1);
    command("f\ne\n");loop();CHECK(!enviandoGravado);
  } else if(test=="leds"){
    command("a\n");loop();loop();CHECK(pins[4]==HIGH&&pins[2]==HIGH);
    command("f\n");loop();CHECK(pins[4]==LOW&&pins[2]==HIGH);
    fakeMillis+=100;loop();CHECK(pins[2]==LOW);
    CHECK(pins.size()==2);
  } else if(test=="coeficiente_invalido"){
    for(double value:{0.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
      payload.fatorCalibracao=value;command("i\n");CHECK(!ensaioAtivo);
    }
    CHECK(LittleFS.writeOpens==0&&count("codigo","COEFICIENTE_INVALIDO")==3);
    payload.fatorCalibracao=1e-310;command("i\n");celula.value=8388607;loop();
    CHECK(!ensaioAtivo&&amostrasGravadas==0&&count("codigo","MEDICAO_INVALIDA")==1);
  } else if(test=="tamanho_amostra"){
    payload.tempo_ms=4294967295UL;payload.empuxo=-1.23456789123e300;amostrasGravadas=4294967295U;
    auto offset=Serial.output.size();informarAmostra();
    CHECK(Serial.output.size()-offset<=140);
    std::cerr<<"Bytes da amostra extrema: "<<Serial.output.size()-offset<<'\n';
  } else CHECK(false);

  // Toda linha emitida deve ser JSON valido, inclusive os erros e eventos.
  messages();
  std::cout<<Serial.output;
  std::cerr<<"PASS "<<test<<'\n';
}
