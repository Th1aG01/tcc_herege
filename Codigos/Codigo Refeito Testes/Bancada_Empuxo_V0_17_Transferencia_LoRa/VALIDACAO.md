# Validação V0.17

## Executado nesta entrega

**141/141 cenários nativos aprovados:** 118 regressões da base V0.16 (seis adaptadas ao novo comportamento deliberado de transferência/cancelamento) e 23 cenários específicos do transporte BT/1. Nenhum caso antigo foi simplesmente omitido do executor.

Os testes incluem o .ino e os três cabeçalhos desta entrega. Compilação g++ C++17 com `-Wall -Wextra -Werror -pedantic`, AddressSanitizer e UndefinedBehaviorSanitizer. ArduinoJson 7.4.2 é real. ESP32, relógio, UART, HX711, LittleFS e rádio são simulados. LeakSanitizer desativado por incompatibilidade com ptrace neste ambiente.

Também passou a verificação de sintaxe com os cabeçalhos oficiais locais LoRa 0.8.0 e HX711 de bogde, mantendo o restante do Arduino simulado. Não há chamada ao método privado `LoRa.isTransmitting()`. O código da biblioteca LoRa foi inspecionado para verificar `onTxDone(nullptr)`, `parsePacket`, `write` e a alternância entre callback TX e polling RX.

**Não houve compilação completa com o toolchain ESP32, upload, rádio físico ou receptor físico.** Nenhuma simulação comprova alcance, temporização RF, durabilidade elétrica ou precisão da célula de carga.

## Cobertura do transporte

| Cenário | Resultado verificado |
| --- | --- |
| Formato e integridade | Envelope, IDs, sequências, CRCs e arquivo reconstruído byte a byte, incluindo metadados |
| CRC de referência | Vetor ASCII 123456789 → 0xCBF43926 |
| Fragmentação | Blocos de 128 bytes; último de 64 bytes no arquivo de duas amostras; tamanhos 90/154/218 |
| Confirmação integral | ACKs avançam blocos; somente CONFIRMAR final encerra com sucesso remoto |
| Fim local sem confirmação | ACK do FIM não basta; mantém envio_local_concluido true e recebimento_confirmado false |
| Sem receptor | Quatro emissões idênticas de INICIO e encerramento limitado, sem alterar o arquivo |
| Timeout e PEDIR | Reenvio idêntico do quadro atual; perda de ACK não duplica blocos confirmados |
| Reparo | Pedido de INICIO, metadados e dados durante espera final; volta a FIM após ACK do reparo |
| Controles inválidos | Magic/versão/IDs/tamanhos/CRC errados, curto, 255 bytes, ACK futuro e CONFIRMAR fora de fase |
| Ruído | Controles inválidos não renovam o prazo; a transferência ainda termina no limite |
| Cancelamento | Preparação, TX e espera; aborta rádio local, fecha arquivo e restaura callback |
| Consulta/deduplicação | Snapshot retido, consultas novas, ID errado, ENVIAR repetido, conflitos e resposta terminal reemitida |
| IDs antigos | CONFIRMAR de outra transferência não conclui a atual |
| Concorrência | Arquivo protegido em espera, consulta permitida, outros arquivos excluíveis, aquisição/envio mutuamente exclusivos |
| Medição preservada | Mudança posterior de coeficiente/tara não recalcula os dados salvos; ao vivo continua JSON |
| Falhas | CRC de amostra antes do envio, leitura durante hash/dados, preparação de TX, endPacket e TX_DONE ausente |
| Limites | Quatro tentativas, 64 repetições/reparos, prazo global de duas horas |
| USB ausente | Envio conclui com ACKs do receptor simulado mesmo com UART bloqueada; consulta posterior recupera resultado |
| Saída | Mensagens JSON dentro dos buffers; STATUS/CONSULTAR_ENVIO com IDs grandes |

Os seis testes adaptados são envio_conclusao, timeout_radio, cal_envio_tara, aq_mesma_medicao, ar_protecao_transmissao e ar_protecao_ultimo_pacote. Antes esperavam amostras gravadas como JSON sem ACK ou cancelamento que deixava TX terminar. Agora exercitam a transferência binária confirmada, preparação assíncrona ou abortamento local de TX conforme a nova especificação.

`quadros_tx` conta inícios de TX; o teste de conclusão usa os eventos de TX_DONE/CONFIRMAR, não esse contador como prova de recebimento. O receptor simulado constrói controles pelo contrato de bytes e verifica o arquivo remontado; não é um receptor pronto para carregar em outra placa.

## Regressões mantidas

Recepção JSON fragmentada, incompleta, desconhecida e malformada; nenhuma execução por caracteres; limites e deduplicação; calibração positiva/negativa/finita; tara com timeout e sem dupla correção; sensor ausente no boot; aquisição e gravação independentes da USB/LoRa; múltiplos arquivos; metadados; blocos; exclusões e proteção; LittleFS sem formatação automática; registros legados e futuros; falhas de escrita/flush/leitura/diretório; espaço insuficiente.

Os testes ar_reboot_gravar e ar_reboot_ler continuam rodando em **processos separados**. O primeiro cria dois ensaios concluídos e um terceiro interrompido, com duas amostras completas e uma parcial. O segundo carrega apenas os bytes dos arquivos antes do setup: confirma os dois concluídos e o incompleto recuperável, preserva metadados/tara, não retoma aquisição, exige nova calibração e cria outro arquivo sem sobrescrever os anteriores. Isso valida a lógica de persistência, não a resistência física a corte de energia.

Armazenamento.h e ComandosArmazenamento.h permanecem byte a byte iguais à base V0.16. O diff do sketch mantém pinos, SPI, frequência, SyncWord, potência, LEDs e baud. O formato de arquivo não foi alterado.

## Ainda verificar na bancada

1. Compilação na Arduino IDE e upload com seu core ESP32 e suas bibliotecas; mesma placa/partições da base.
2. Montagem do LittleFS existente, vários ensaios antes/depois de reset e ausência de sobrescrita.
3. TX_DONE por DIO0, LED amarelo durante TX, apagado na espera, cancelamento de TX e retorno ao envio ao vivo.
4. Transição half-duplex TX/RX, guarda inicial de 50 ms no receptor e timeout de 2000 ms com o tempo real dos quadros.
5. Receptor BT/1: ACK, duplicatas, PEDIR, validação/persistência integral e CONFIRMAR; perdas de cada tipo de quadro, CRC incorreto e estação reiniciando.
6. Comparação byte a byte entre arquivo original e recebido, inclusive nomes UTF-8, coeficiente negativo, tara e kgf.
7. Aquisição real, carga conhecida, taxas/latências, LEDs e gravação autônoma com USB desconectada. Conferir se reconexão provoca reset por DTR/RTS.
8. Queda de energia real e durabilidade do LittleFS; consumo de RAM e operação prolongada. O mock não reproduz cache, desgaste, fragmentação nem o custo síncrono de cada chamada de arquivo.

O receptor e a interface Python não foram modificados. Sem receptor BT/1, ENVIO_SEM_CONFIRMACAO é o resultado esperado quando o rádio local transmite normalmente; isso é diferente de LORA_TX_TIMEOUT, que indica falta de TX_DONE local.

## Reproduzir os testes nativos

Na raiz extraída, com g++, Python 3 e ArduinoJson 7.4.2:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -no-pie \
  -Ivalidacao_nativa/mocks -I/caminho/ArduinoJson-7.4.2/src \
  validacao_nativa/test_firmware.cpp -o validacao_nativa/test_firmware
python3 validacao_nativa/run_tests.py
```

O executor cria disco temporário para o teste entre processos. Os resultados exatos estão em validacao_nativa/resultados.json e cada .jsonl contém a saída USB daquela simulação. Não há comandos de injeção de falha no firmware; essas falhas só existem nos mocks. Nenhum executável, biblioteca de terceiros ou arquivo temporário de disco está no pacote.

Referências de API inspecionadas nos arquivos locais oficiais:

- https://github.com/sandeepmistry/arduino-LoRa/blob/0.8.0/src/LoRa.h
- https://github.com/sandeepmistry/arduino-LoRa/blob/0.8.0/src/LoRa.cpp
- https://github.com/bogde/HX711/blob/master/src/HX711.h
