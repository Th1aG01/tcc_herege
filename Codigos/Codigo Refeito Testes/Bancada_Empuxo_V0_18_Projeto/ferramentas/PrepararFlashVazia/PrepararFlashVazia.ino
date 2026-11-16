#include <Arduino.h>
#define BANCADA_PROVISIONAMENTO
#include "BackendExterno.h"
char entrada[80] = {};
size_t usados = 0;
bool descartar = false;
void setup() {
  Serial.begin(115200);
  pinMode(FLASH_CS, OUTPUT); digitalWrite(FLASH_CS, HIGH);
  pinMode(SPI_CS_RADIO, OUTPUT); digitalWrite(SPI_CS_RADIO, HIGH);
  SPI.begin(SPI_SCK_BANCADA, SPI_MISO_BANCADA, SPI_MOSI_BANCADA, SPI_CS_RADIO);
  FlashEnsaios.nor.escrita = false;
  FlashEnsaios.nor.begin();
  Serial.printf("JEDEC: %06lX; %s\n", (unsigned long)FlashEnsaios.nor.jedec, FlashEnsaios.nor.erro);
  Serial.println("Boot somente leitura. Ignicao deve estar fisicamente desabilitada.");
  Serial.println("Apenas para chip autorizado e integralmente vazio (2 MiB = FF).");
  Serial.println("Para autorizar criacao do LittleFS externo, enviar exatamente:");
  Serial.println("AUTORIZO_INICIALIZAR_FLASH_VAZIA_EF4015");
  Serial.println("Qualquer byte diferente de FF recusa a operacao; nao existe comando para forcar.");
}
void loop() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      entrada[usados] = 0;
      if (!descartar && !strcmp(entrada, "AUTORIZO_INICIALIZAR_FLASH_VAZIA_EF4015")) {
        FlashEnsaios.nor.escrita = true;
        bool ok = FlashEnsaios.prepararVazia(true);
        FlashEnsaios.nor.escrita = false;
        Serial.printf("Resultado: %s; driver=%s; erro_fs=%d\n",
          ok ? "PREPARADO" : "RECUSADO_OU_FALHOU", FlashEnsaios.nor.erro, FlashEnsaios.ultimoErro);
        Serial.println("Nao repetir a inicializacao. Reiniciar com firmware principal para validar.");
      } else Serial.println("Nenhuma operacao autorizada.");
      usados = 0; descartar = false;
    } else if (c < 32 || c > 126 || usados >= sizeof(entrada)-1) descartar = true;
    else if (!descartar) entrada[usados++] = (char)c;
  }
}
