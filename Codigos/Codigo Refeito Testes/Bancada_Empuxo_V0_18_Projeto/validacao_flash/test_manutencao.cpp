#include <iostream>
#include <cstdlib>
#define BANCADA_SPI_NOR
#ifdef TEST_DIAGNOSTICO
#include "../ferramentas/DiagnosticoFlash/DiagnosticoFlash.ino"
#else
#include "../ferramentas/PrepararFlashVazia/PrepararFlashVazia.ino"
#endif
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(0)
int main(int argc,char **argv){
  CHECK(argc==2);std::string caso=argv[1];
  if(caso=="conteudo")SPI.bytes[500]=0;
  auto antes=SPI.bytes;setup();CHECK(SPI.bytes==antes&&!SPI.programs&&!SPI.erases);
#ifndef TEST_DIAGNOSTICO
  Serial.feed("nao_autorizo\n");loop();CHECK(SPI.bytes==antes);
  Serial.feed(std::string(100,'X')+"AUTORIZO_INICIALIZAR_FLASH_VAZIA_EF4015\n");loop();CHECK(SPI.bytes==antes);
  Serial.feed("AUTORIZO_INICIALIZAR_FLASH_VAZIA_EF4015\n");loop();
  if(caso=="conteudo")CHECK(SPI.bytes==antes&&!SPI.programs&&!SPI.erases);
  else {
    CHECK(FlashEnsaios.montado&&FlashEnsaios.exists("/ensaios")&&!FlashEnsaios.nor.escrita);
    auto depois=SPI.bytes;auto mut=SPI.mutation;
    Serial.feed("AUTORIZO_INICIALIZAR_FLASH_VAZIA_EF4015\n");loop();CHECK(SPI.bytes==depois&&SPI.mutation==mut);
  }
#endif
  std::cout<<Serial.output;std::cerr<<"PASS "<<caso<<'\n';
}
