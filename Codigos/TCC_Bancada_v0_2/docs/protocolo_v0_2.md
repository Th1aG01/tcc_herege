# Protocolo v0.2 — TCC2

## Transporte e temporização

- Bancada: ESP32 → USB UART, 115200 baud, 8N1, linhas ASCII terminadas por LF. Comandos também usam LF; CR é ignorado.
- Estação: saída BRAVO original, 115200 baud, `Received packet '<payload>' with RSSI <inteiro>`.
- Perfil de rádio: 915000000 Hz, sync word 145 (0x91), SF7, BW125000 Hz, CR4/5, preâmbulo 8, cabeçalho explícito, IQ normal, CRC de rádio desligado como no exemplo de referência. O payload possui CRC16 de aplicação. Potência de bancada 2 dBm; SPI 1 MHz.
- Os valores explicitados no transmissor correspondem ao perfil padrão SX1276 usado pelo exemplo BRAVO fornecido. Uma estação com firmware diferente deve ser conferida, não apenas presumida compatível.
- HX711: canal A, ganho 128, DT33/CLK32. Sem tara, média ou escala no firmware. O momento de leitura é marcado depois de `read()` por `esp_timer_get_time()`, em microssegundos relativos ao início. Não é um timestamp de hardware da conversão ADC.
- Transmissão não bloqueante, com confirmação local pela interrupção DIO0/GPIO26 e timeout de 2 s. A última leitura é enviada em até 2 pacotes/s; leituras intermediárias permanecem no USB/arquivo local. A frequência efetiva do HX711 depende de sua configuração física e do tempo gasto na leitura/gravação.
- Sem leitura pronta por mais de 2 s durante um ensaio: encerra por `HX711_TIMEOUT`; nunca gera uma amostra zero como substituição.

## Amostra

`TCC2;session;packet_seq;sample_seq;t_us;raw;flag;crc16`

| Campo | Definição |
|---|---|
| `session` | 16 caracteres hexadecimais minúsculos, gerados pelo PC |
| `packet_seq` | `-` no USB/flash; inteiro começando em 0 nos envios LoRa |
| `sample_seq` | Inteiro começando em 0, incrementado a cada leitura adquirida |
| `t_us` | Microssegundos desde o início; contador de 64 bits |
| `raw` | Inteiro com sinal entre -8388608 e 8388607 |
| `flag` | `SAT` nos dois extremos do ADC; `OK` nos demais |
| `crc16` | CRC-16/CCITT-FALSE, polinômio 0x1021, inicial 0xFFFF, sem reflexão, sem XOR final; quatro hexadecimais |

O CRC cobre todos os bytes ASCII desde `TCC2` até `flag`, incluindo seus separadores, mas não o ponto e vírgula anterior ao CRC. Em Python equivale a `binascii.crc_hqx(body.encode('ascii'), 0xffff)`.

CRC detecta corrupção acidental; não autentica a origem nem criptografa a comunicação. O CRC e os contadores não garantem que toda amostra gerada pelo ADC foi lida pelo ESP32.

O aplicativo rejeita pacotes inválidos, não mistura sessões diferentes e guarda CSVs separados para os dois transportes. Sequências iguais com valores diferentes são sinalizadas. O gráfico LoRa usa pontos; não cria valores intermediários. Lacunas no USB interrompem a linha do gráfico.

## Comandos da bancada

| Comando | Efeito / resposta |
|---|---|
| `STATUS` ou `HELLO` | `STATUS;IDLE ou RECORDING;session ou -;count;OK ou UNAVAILABLE;free_bytes;ON/OFF/FAIL;OK/SAT/WAIT` |
| `START <session> <segundos> <flash>` | Limite inteiro 5–3600 s; flash 0/1. Responde `ACK;START;<session>` após validar sensor e abrir arquivo, quando solicitado |
| `STOP <session>` | Encerra a aquisição correspondente; `ACK;STOP;<session>;<motivo>;<count>` |
| `RADIO ON` / `RADIO OFF` | Habilita/pausa novos pacotes; o pacote em andamento termina. USB e flash continuam |
| `PREPARE_FLASH CONFIRM` | Somente ocioso. Tenta preparar LittleFS com formatação se a montagem falhar. Nunca enviado automaticamente |
| `LIST` | Lista `FILE_INFO;<session>;<bytes>` e `FILE_LIST_END` |
| `GET <session>` | Exporta arquivo somente quando ocioso |
| `DELETE <session>` | Exclui somente `/tcc_<session>.log`, somente ocioso |

Comandos rejeitados respondem `ERR;<motivo>`. Não existe comando de ignição, atuador ou seleção automática de GPIO de saída.

START repetido com a mesma sessão ativa não reinicia a aquisição. Um arquivo existente nunca é truncado para iniciar outra sessão. STOP repetido da última sessão repete sua confirmação. Um novo START é rejeitado enquanto ainda houver transmissão anterior em andamento.

## Persistência e falhas

Registro interno: `/tcc_<session>.log`, começando por `META;TCC2;<session>`, seguido por amostras USB TCC2 e terminado por `END;<session>;<motivo>;<count>` quando possível. Gravação por amostra; flush aproximadamente a cada segundo e no encerramento. Não é garantia de proteção absoluta contra corte de energia. Sem sobrescrever arquivos antigos ou apagar ao encher.

Motivos de encerramento: `MANUAL`, `LIMIT`, `HX711_TIMEOUT`, `FLASH_FULL`, `FLASH_WRITE`. Timeout/falha do LoRa sinaliza o problema e mantém a aquisição USB/flash. Perda de USB não envia STOP automaticamente no ESP32: o registro interno continua até o limite ou uma falha detectada. Ao reconectar, STATUS informa se há aquisição em andamento, permitindo encerrá-la.

Download: `FILE_BEGIN;<session>;<bytes>`, linhas `FILE_DATA;<linha-original>`, `FILE_END;<session>;<bytes>`. Os bytes originais usam LF. Cópias em andamento têm sufixo `.partial`; só são concluídas após validar tamanho, CRC e sequência. Um arquivo internamente incompleto, mas com linhas válidas, pode ser aberto com aviso. Uma linha parcial/corrompida exige análise posterior; não é reparada silenciosamente.

No PC, os CSVs são gravados incrementalmente e sincronizados aproximadamente a cada segundo. Metadados JSON são substituídos atomicamente. Estatísticas distinguem amostras USB, pacotes LoRa, lacunas, duplicatas, saturação e comparação dos caminhos. O total informado pelo ESP32 é registrado para comparação com as amostras recebidas no PC.

## Referências técnicas

- [API oficial Arduino LoRa](https://github.com/sandeepmistry/arduino-LoRa/blob/master/API.md)
- [Implementação oficial Arduino LoRa](https://github.com/sandeepmistry/arduino-LoRa/blob/master/src/LoRa.cpp)
- [HX711 por bogde](https://github.com/bogde/HX711)
- [LittleFS no Arduino-ESP32](https://github.com/espressif/arduino-esp32/blob/master/libraries/LittleFS/src/LittleFS.h)
- [API pySerial](https://pyserial.readthedocs.io/en/latest/pyserial_api.html)

O código original da estação BRAVO fornecido pelo usuário foi preservado no pacote.
