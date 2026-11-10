# Tempo do ensaio e interrupções — v0.3.1

## Problema relatado e alcance do ajuste

O usuário relatou aquisições programadas para 30 segundos que terminavam em tempos variáveis, como 3, 7 e 12 segundos. Não foi disponibilizado o registro de um desses ensaios; portanto **a causa física ou lógica da interrupção original permanece não confirmada**.

Na revisão da v0.3.0, o campo TEMPO mostrava o instante da última amostra, e o encerramento por falha tinha apresentação semelhante ao encerramento normal. O firmware já utilizava o relógio monotônico do ESP32 para a duração. A v0.3.1 corrige a apresentação, reforça a checagem do prazo e acrescenta evidências para o diagnóstico.

## Comportamento

- A contagem começa quando o ESP32 aceita START, após abrir o registro interno, se solicitado. Não inclui o tempo anterior de comunicação/preparação.
- O relógio é `esp_timer_get_time()`, em microssegundos, com cálculo de 64 bits relativo ao início de cada ensaio. Não depende do relógio civil do Windows, do número de amostras, da carga ou dos pacotes LoRa.
- O limite é conferido em cada volta do programa, antes dos comandos e antes/depois da leitura do sensor. Leituras que ultrapassam a janela não entram no ensaio.
- O instante de parada é congelado antes de fechar o arquivo. O programa não substitui esse valor pelo prazo solicitado: eventual atraso de execução permanece no valor medido.
- As atualizações do tempo são enviadas por USB aproximadamente a cada 250 ms, também nas respostas STATUS e no encerramento. A interface exibe o último tempo efetivamente informado, sem avançar um cronômetro fictício quando perde a conexão.
- A interface aguarda brevemente eventuais pacotes finais após o fim da aquisição. Essa espera não aumenta a duração registrada.
- Um CLOCK válido da sessão ativa também confirma o início, permitindo recuperar a perda do ACK de início.
- Retirar a carga ou obter leitura zero não causa encerramento automático.

## Mensagens de encerramento

| Situação | Apresentação / diagnóstico |
|---|---|
| Prazo alcançado e duração confirmada | DURAÇÃO ATINGIDA |
| Comando de parada recebido pelo ESP32 | ENCERRADO POR COMANDO |
| HX711 sem nova leitura por mais de 2 s | INTERROMPIDO, motivo HX711_TIMEOUT |
| Memória cheia ou falha de escrita | INTERROMPIDO, motivo FLASH_FULL ou FLASH_WRITE |
| USB perdido ou ESP32 reiniciado | INTERRUPÇÃO, encerramento não confirmado |
| Início sem confirmação por 7 s | O PC solicita parada e registra start_confirmation_timeout, distinguindo-a da ação do usuário |
| ESP32 informa LIMIT antes do prazo em seu próprio relógio | INCONSISTÊNCIA DE TEMPO; estado timing_error |

A ausência de confirmação USB não prova que o ESP32 parou: sua aquisição pode continuar até o limite. Uma falha do LoRa não deve encerrar a aquisição USB. O registro continua identificando dados brutos, sem calibração.

## Protocolo adicional USB

```
CLOCK;<session>;<RECORDING|STOPPED>;<elapsed_us>;<limit_us>
```

`STOPPED` fornece o tempo congelado no instante de parada. A confirmação e seu motivo continuam em `ACK;STOP;<session>;<reason>;<count>`. STATUS e STOP repetido permitem recuperar o relógio final da última sessão. Um novo ensaio usa outro identificador e zera a contagem.

O aplicativo rejeita tempo regressivo, limite diferente do solicitado e alteração do tempo após STOPPED. Ignora o relógio de outra sessão. As mensagens ficam em `eventos.jsonl`; o resumo final fica em `ensaio.json`:

- `max_duration_s`: duração solicitada, mantida para compatibilidade.
- `timing.requested_duration_s`: duração solicitada.
- `timing.accepted_duration_us`: limite confirmado pelo ESP32.
- `timing.last_elapsed_us`: último tempo informado, que não é necessariamente a duração final.
- `timing.actual_duration_us`: duração final somente quando o relógio STOPPED e a confirmação de parada foram recebidos.
- `timing.duration_confirmed`: indica se a duração final foi confirmada.
- `last_sample_us`: horário da última amostra, separado por USB/LoRa.
- `reason`: motivo informado no encerramento.
- `stop_requested_by`: origem de um pedido de parada pelo PC, quando houver.

O formato dos CSV, das amostras TCC2, dos arquivos internos do ESP32 e da transferência TCCF3 foi preservado. O novo relógio é diagnóstico por USB; não é acrescentado ao arquivo interno transmitido por LoRa nesta atualização. Arquivos antigos continuam abrindo e não recebem uma duração inventada. Se só o aplicativo for atualizado, o relógio pode aparecer como não informado: grave também o firmware v0.3.1.

## Teste na protoboard

1. Preserve os registros anteriores. Extraia a versão nova em outra pasta e grave o firmware correspondente na bancada.
2. Feche o Monitor Serial; abra INICIAR.bat, conecte a bancada por USB e mantenha Sem estação.
3. Programe 30 s e mantenha a gravação interna, se disponível. Inicie, aplique uma carga pequena e depois retire-a.
4. Confira TEMPO DO ENSAIO e a mensagem final. A última amostra normalmente antecede o instante de parada por até aproximadamente um intervalo de aquisição; isso é diferente de encerrar cedo.
5. Repita três vezes, sem precisar reiniciar o ESP32. Confira que cada teste começa do zero.
6. Se houver interrupção, envie ensaio.json e eventos.jsonl do mesmo teste, além de uma captura da mensagem se possível. Não apague o original interno.

Só após identificar o motivo vale investigar a parte correspondente: alimentação/cabeamento do HX711, reinício da placa, armazenamento, USB ou confirmação de início. Não é necessário trocar fios ou calibrar a célula apenas para verificar a duração.
