# Envio de registro após o ensaio — TCCF3

## Arquitetura

O PC controla a bancada por USB. Após encerrar a aquisição e fechar o registro interno, um comando USB solicita que o ESP32 envie esse arquivo pelo rádio. A estação BRAVO imprime os pacotes recebidos no USB de outro notebook, onde o aplicativo receptor reconstrói e verifica o registro.

Não há comando de aquisição vindo pelo rádio, nem ACK automático da estação. Conclusão local da transmissão e recebimento íntegro do arquivo são estados distintos. A camada anterior de aquisição TCC2 permanece em `protocolo_v0_2.md`.

## Alterações da versão 0.3

- USB é a origem inicial do gráfico; a estação não é necessária para iniciar o ensaio.
- Telemetria ao vivo inicia pausada no firmware.
- Aplicativo independente: `main.py --receptor` / `RECEPTOR_LORA.bat`.
- Novos comandos USB: `LORASEND <session>` e `LORACANCEL`.
- STATUS informa `TRANSFER` durante envio e `FILE_TX` no campo do rádio.
- A memória deve estar montada e o arquivo presente; a aquisição deve estar encerrada. Não é necessária nova leitura do HX711 para enviar um registro antigo.
- START/GET/DELETE são recusados durante envio. O cancelamento preserva o arquivo e permite que um pacote já iniciado termine.

## Quadro de rádio

`TCCF3;session;kind;index;blocks;size;file_crc32;data64;frame_crc16`

| Campo | Significado |
|---|---|
| session | 16 hexadecimais minúsculos; identificador do registro |
| kind | S: início da passagem; B: bloco; E: fim da passagem |
| index | Índice do bloco a partir de 0; zero em S/E |
| blocks | Total de blocos, igual a ceil(size/120) |
| size | Tamanho original, de 1 a 16 MiB |
| file_crc32 | CRC32/ISO-HDLC do arquivo completo; 8 hexadecimais |
| data64 | Até 120 bytes originais em Base64; vazio em S/E |
| frame_crc16 | CRC-16/CCITT-FALSE do texto até data64, sem o separador anterior ao CRC; 4 hexadecimais |

CRC32 equivale a `zlib.crc32(bytes) & 0xffffffff`. O firmware usa o polinômio refletido 0xedb88320, inicial 0xffffffff e XOR final 0xffffffff. CRC16 equivale a `binascii.crc_hqx(body.encode('ascii'), 0xffff)`.

Os quadros ficam abaixo de 255 bytes. Cada bloco repete os metadados, permitindo receber mesmo sem o marcador inicial. São enviados dois S, todos os B e três E, com intervalo mínimo de 400 ms entre inícios de pacotes. O envio é assíncrono por DIO0 e tem timeout local de 2 s.

Mensagens USB: `LORA_FILE;START;session;blocks;size;crc32`, `LORA_FILE;PROGRESS;session;done;blocks`, `LORA_FILE;SENT;session;blocks` ou `LORA_FILE;CANCELLED;session;reason`.

`SENT` só confirma a conclusão local dos envios. Perfil de rádio: 915 MHz, sync 145, SF7, BW125 kHz, CR4/5, preâmbulo 8, cabeçalho explícito e IQ normal. O código BRAVO receptor fornecido não foi modificado.

## Recepção

Aceita o invólucro BRAVO `Received packet '...' with RSSI ...` ou uma linha TCCF3 direta. Valida CRC16, tamanho, sessão e índice antes de gravar o bloco. A lista de índices persistidos só é atualizada depois da sincronização dos bytes em disco.

Após todos os blocos, verifica tamanho e CRC32, depois o conteúdo TCC2: cabeçalho, sequência, CRC das amostras e marcador final quando presente. Só então publica `registro.log`, exporta `amostras.csv` e mostra a curva. Arquivos com identificação, tamanho ou CRC32 diferentes não são misturados.

Reenviar o mesmo registro preenche faltas. Duplicatas são conferidas contra a cópia local antes de serem ignoradas. Não há retransmissão automática solicitada pela estação. Ao reabrir o aplicativo na mesma pasta, `recepcao.json` e `registro.log.partial` permitem continuar a partir do progresso sincronizado.

Transferência íntegra não implica aquisição válida metrologicamente. Registro original sem END continua identificado como aquisição incompleta. Arquivos corrompidos são preservados e não apresentados como completos. O nome descritivo, as notas e a referência visual do PC não fazem parte do arquivo interno transmitido nesta versão.

CRC não autentica a origem nem criptografa o conteúdo. Taxa, alcance e perdas ainda precisam ser medidos na montagem.

Referências: [API LoRa](https://github.com/sandeepmistry/arduino-LoRa/blob/master/API.md), [biblioteca LoRa 0.8.0](https://github.com/sandeepmistry/arduino-LoRa/tree/0.8.0), [Arduino-ESP32](https://github.com/espressif/arduino-esp32).
