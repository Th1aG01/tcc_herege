# Protocolo USB 2 — firmware V0.14

## Transporte e enquadramento

- UART USB, **115200 baud**, configuração padrão 8N1.
- Um objeto JSON por linha, terminado por **LF**. CRLF também é aceito. CR sozinho não conclui a linha.
- Limite de **512 bytes antes do LF**, incluindo espaços e um eventual CR. Buffer fixo; não cresce com a entrada.
- Mensagens podem chegar em qualquer divisão de fragmentos. Nenhuma ação ocorre antes do LF.
- Após **1000 ms sem ler um novo byte** de uma linha parcial, o ESP emite `RX_TIMEOUT` e descarta até o próximo LF. Ao retomar depois de timeout, envie primeiro um LF isolado e depois repita a solicitação inteira.
- Excesso de tamanho ou controle inválido também descarta toda a linha até LF; seu restante nunca vira outro comando. Linhas vazias/de espaços são ignoradas.
- São lidos no máximo 64 bytes e processada no máximo uma linha por passagem do loop. Não há `readStringUntil`, espera por entrada, execução por caracteres ou `Serial.flush()`.
- A saída usa fila fixa de 8192 bytes e escreve no máximo 64 bytes por passagem, limitados por `Serial.availableForWrite()`. Uma linha é enfileirada inteira, sem cortes deliberados no meio de um JSON.

A aplicação só produz mensagens JSON. ROM, bootloader e logs habilitados no core podem produzir texto durante o boot; eles não fazem parte do protocolo. A sessão de aplicação é identificada com `IDENTIFICAR`.

## Envelope de comando

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":2,"comando":"INICIAR"}
```

| Campo | Regra |
| --- | --- |
| `protocolo` | Inteiro 2 |
| `tipo` | String `comando` |
| `sessao` | String de 16 caracteres devolvida pelo ESP na descoberta |
| `id` | Inteiro sem sinal, 1 a 4294967295, crescente para novas solicitações |
| `comando` | Nome em maiúsculas, de 1 a 24 caracteres, somente A–Z e `_` |

Os cinco campos são obrigatórios na operação normal. CONFIGURAR_CALIBRACAO exige também o campo numérico coeficiente. Outros campos extras, chaves repetidas, tipos errados e nomes abreviados são rejeitados. A ordem dos campos e espaços externos não alteram a identidade da solicitação. Não existe campo de offset. Somente CONFIGURAR_CALIBRACAO recebe parâmetro nesta versão; veja CALIBRACAO_E_TARA.md.

O envelope é plano. O campo `coeficiente` aceita número JSON com sinal negativo, casas decimais e notação científica. `id` e `protocolo` continuam exigindo tipos inteiros sem sinal. Chaves extras, objetos/arrays aninhados, booleanos, null, números entre aspas no coeficiente e sintaxe numérica inválida são rejeitados. Strings seguem a sintaxe JSON; escapes válidos são decodificados. Um LF literal encerra a linha: não use JSON em várias linhas.

### Descoberta

A única exceção ao envelope normal é o comando de leitura abaixo, sem `sessao` e com `id: 0`:

```json
{"protocolo":2,"tipo":"comando","id":0,"comando":"IDENTIFICAR"}
```

Ele pode ser repetido, não executa ações e não consome IDs. Retorna identificação do dispositivo, firmware, protocolo, sessão atual, maior ID consumido e limites. Depois de conectar/reconectar, consulte-o e use `maior_id + 1` para a próxima solicitação nova, respeitando o limite numérico. Se o limite máximo for atingido, uma nova sessão requer reinicialização depois de finalizar o trabalho.

## Operações existentes

| `comando` | Resposta de sucesso | Efeito |
| --- | --- | --- |
| `IDENTIFICAR` | `concluido` | Identidade e limites; aceita descoberta especial ou envelope normal |
| `STATUS` | `concluido` | Estado atual de aquisição, sensor, calibração, armazenamento e rádio |
| `AJUDA` | `concluido` | Lista dos comandos |
| `INICIAR` | `aceito` | Abre nova aquisição/gravação sem LoRa ao vivo |
| `INICIAR_LORA` | `aceito` | Abre aquisição/gravação com LoRa ao vivo |
| `FINALIZAR` | `concluido` | Encerra aquisição; se já ociosa, confirma sem iniciar outra ação |
| `LORA_LIGAR` | `concluido` | Habilita envio ao vivo durante aquisição |
| `LORA_DESLIGAR` | `concluido` | Desabilita próximos envios ao vivo; aquisição continua |
| `LORA_ALTERNAR` | `concluido` | Alterna o envio ao vivo; repetição do mesmo ID não alterna de novo |
| `ENVIAR` | `aceito` | Inicia envio do arquivo gravado; exige aquisição encerrada |
| `CANCELAR` | `concluido` | Cancela próximos pacotes do envio gravado |
| `CONFIGURAR_CALIBRACAO` | `concluido` | Configura coeficiente em kgf e invalida tara; bloqueado durante aquisição/tara |
| `CONSULTAR_CALIBRACAO` | `concluido` | Consulta coeficiente, validade e prontidão da tara |
| `TARA` | `aceito` | Coleta dez leituras em até três segundos; bloqueada durante aquisição |

Todos os comandos chegam pela USB. `FINALIZAR` não cancela envio gravado; `CANCELAR` não encerra aquisição. Um pacote já em andamento pode terminar depois do cancelamento. As confirmações do LoRa são locais, sem ACK da estação remota.

## IDs, repetição e limites da garantia

O computador deve manter **um contador crescente por sessão** e enviar novas solicitações em ordem. Gaps são aceitos; um ID menor que um ID já consumido não poderá ser usado para uma nova operação. A proteção é compartilhada por todos os comandos, inclusive consultas e comandos desconhecidos com envelope válido. Recomenda-se um único controlador da porta.

| Situação | Comportamento |
| --- | --- |
| Mesmo ID, mesma sessão e mesmo comando e coeficiente, resposta ainda retida | Reenvia a resposta original, sem executar novamente; também reenvia o evento terminal se disponível |
| Mesmo ID retido, outro comando ou coeficiente | `ID_CONFLITO`; nenhuma ação |
| ID menor/igual ao maior consumido, já retirado do histórico | `ID_EXPIRADO`; nenhuma ação, mesmo que o texto pareça uma nova solicitação |
| Sessão diferente | `SESSAO_INVALIDA`; nenhuma ação |
| Envelope inválido | Rejeição, sem reservar o ID; pode ser corrigido e reenviado |
| Comando desconhecido com envelope válido | Erro retido; ID consumido |

São mantidos **oito registros em RAM**. Registros de operações em andamento ficam protegidos contra remoção. Os demais são substituídos a partir do menor ID. Resposta de aceitação e evento terminal pertencem ao ID que iniciou a operação, mesmo quando outra solicitação a finalizou/cancelou.

As respostas repetidas preservam seus valores e horários originais. Portanto, repetir um `STATUS` não gera uma consulta nova; use outro ID para obter estado atualizado. Erros de execução também são repetidos: uma operação inicialmente rejeitada não é tentada de novo com o mesmo ID depois que o estado muda.

**Em caso de dúvida sobre entrega, repita a mesma solicitação com o mesmo ID.** Trocar o ID significa pedir uma nova operação. Um ID expirado não será executado novamente; consulte o estado com novo ID para decidir o próximo passo.

O histórico não é persistido na flash. Ao reiniciar, uma nova sessão é gerada e os IDs antigos, acompanhados da sessão antiga, são rejeitados. Isso não recupera operações interrompidas por falta de energia nem constitui autenticação criptográfica. Após reset, o controlador deve redescobrir a sessão e consultar o estado, sem converter automaticamente comandos pendentes em novas solicitações.

## Mensagens do ESP

Mensagens de controle compartilham `protocolo`, `tipo`, `sessao`, `ms` e `id`. `ms` é o contador desde o boot, não horário de calendário; pode dar a volta no contador de 32 bits. `id` é null quando a mensagem não está associada a uma solicitação identificável. `comando` aparece nas respostas e nos eventos gerados diretamente durante seu processamento.

### Resposta

```json
{"protocolo":2,"tipo":"resposta","sessao":"0123456789ABCDEF","ms":5000,"id":2,"comando":"INICIAR","ok":true,"fase":"aceito","estado":"ADQUIRINDO","lora_ao_vivo":false}
```

`aceito` indica que a operação prolongada foi iniciada. **Não significa que a aquisição ou a transmissão terminou.** Para comandos instantâneos e consultas, a fase é `concluido`. Não se envia uma resposta de sucesso quando uma ação é recusada.

### Amostra

```json
{"protocolo":2,"tipo":"amostra","ensaio_id":2,"tempo_ms":100,"empuxo_kgf":1.5,"indice":1}
```

`ensaio_id` corresponde ao ID do comando que iniciou a aquisição; `indice` começa em 1. Somente amostras gravadas com sucesso geram essa mensagem. O valor ilustrado pressupõe coeficiente calibrado, não o coeficiente padrão 1.0.

Para reduzir tráfego a 115200 baud, amostras omitem sessão, `ms`, identificação física e configuração repetidos. O controlador associa as amostras à sessão USB atual e consulta coeficiente/unidade via `STATUS`. Deve encerrar essa associação ao detectar um novo boot/sessão.

Quando a fila USB enche, amostras de telemetria podem ser descartadas para reservar espaço a controle. Isso não descarta a amostra já gravada em flash. Lacunas de `indice` e `usb.amostras_descartadas` tornam essa situação observável. Não há garantia de entrega de todas as amostras pela USB sob consumidor lento.

### Eventos de estado e conclusão

Eventos informativos têm `codigo`, `descricao` e `estado`, por exemplo `BOOT`, `USB_PRONTA`, `TARA_CONCLUIDA` e `ESTADO_ATUALIZADO`.

Ao encerrar uma operação prolongada, o evento inclui o ID original, a fase terminal e contadores:

```json
{"protocolo":2,"tipo":"evento","sessao":"0123456789ABCDEF","ms":9000,"id":2,"causado_por":3,"codigo":"ENSAIO_ENCERRADO","fase":"concluido","descricao":"Finalizado por comando.","estado":"OCIOSO","amostras_gravadas":40,"amostras_enviadas":0}
```

Neste exemplo, o comando de ID 3 (`FINALIZAR`) encerrou a operação de ID 2. A resposta do comando 3 é outra mensagem. O evento terminal pode aparecer antes da resposta do comando de parada/cancelamento.

| Código terminal | Fases possíveis |
| --- | --- |
| `ENSAIO_ENCERRADO` | `concluido` por comando; `falhou` por ausência de sensor, erro de cálculo ou gravação |
| `ENVIO_CONCLUIDO` | `concluido`, após terminar o último pacote local |
| `ENVIO_CANCELADO` | `cancelado`, após interromper o agendamento dos próximos pacotes |
| `TARA_CONCLUIDA` | `concluido`, após aplicar a média de dez leituras brutas |
| `TARA_FALHOU` | `falhou`, em timeout, sem aplicar média parcial |
| `ENVIO_INTERROMPIDO` | `falhou`, por leitura, preparação/transmissão ou timeout TX_DONE |

Um erro assíncrono pode ter `id: null`; o evento terminal identifica a operação afetada. Receber novamente um evento terminal idêntico após uma repetição é esperado; o computador também deve tratar eventos repetidos pelo ID e código.

### Erro e aviso

```json
{"protocolo":2,"tipo":"erro","sessao":"0123456789ABCDEF","ms":5200,"id":5,"comando":"INICIAR","ok":false,"fase":"rejeitado","nivel":"ERRO","codigo":"ENSAIO_ATIVO","origem":"ESP32","descricao":"Ja existe um ensaio em andamento."}
```

Erros contêm `codigo`, `origem` e `descricao`. `fase` é `rejeitado` quando existe comando identificado no processamento, ou `falha` para falhas sem esse contexto. Erros de enquadramento/JSON não tentam extrair um ID de uma linha inválida.

Avisos usam `tipo: aviso`, `nivel: AVISO`, origem, código e descrição. O aviso `COEFICIENTE_PADRAO` informa que o valor inicial não foi explicitamente configurado e não libera o ensaio.

Em falta de memória para serializar, a mensagem fixa de emergência `JSON_MEMORIA` pode omitir sessão, tempo, fase e nível. Nenhum sistema de logs consegue assegurar entrega com falta de RAM ou transporte indisponível.

### Consulta de estado

| Grupo | Conteúdo |
| --- | --- |
| Identificação | Dispositivo, firmware, protocolo e sessão |
| `aquisicao` | Ativa, ID da operação e número de amostras gravadas |
| `sensor` | Leituras recentes, tara pronta e tara inicial em andamento |
| `calibracao` | kgf, coeficiente, origem padrão/USB, configuração explícita, validade e prontidão; validação física a cargo do operador |
| `tara` | Pronta/em andamento, progresso e timeout |
| `armazenamento` | Inicialização, LittleFS interno e limite de um ensaio |
| `radio` | Inicialização, TX, modo ao vivo, envio gravado, ID e progresso |
| `usb` | Maior ID consumido, amostras USB descartadas e mensagens de controle descartadas |

`OCIOSO` não significa que todos os componentes estão disponíveis. Os contadores são da sessão atual; após reiniciar, não enumeram automaticamente o conteúdo do arquivo existente. O total do arquivo é lido ao executar `ENVIAR`. `radio.pronto` não comprova recepção remota. `calibracao.validacao_fisica` não deve ser confundida com validação matemática do coeficiente.

## Códigos de erro

| Grupo | Códigos |
| --- | --- |
| Enquadramento | `RX_TIMEOUT`, `RX_TAMANHO`, `RX_CARACTERE` |
| Envelope | `JSON_INVALIDO`, `ID_INVALIDO`, `COMANDO_INVALIDO`, `PROTOCOLO_INVALIDO`, `TIPO_INVALIDO`, `CAMPO_DESCONHECIDO` |
| Sessão e repetição | `SESSAO_INVALIDA`, `ID_CONFLITO`, `ID_EXPIRADO`, `REQUISICOES_OCUPADAS` |
| Operação | `COMANDO_DESCONHECIDO`, `ENSAIO_ATIVO`, `ENSAIO_INATIVO`, `ENVIO_ATIVO` |
| Sensor/calibração | `HX711_TARA_TIMEOUT`, `HX711_TARA_INICIAL_TIMEOUT`, `TARA_NAO_PRONTA`, `TARA_EM_ANDAMENTO`, `CALIBRACAO_NAO_CONFIGURADA`, `HX711_INDISPONIVEL`, `HX711_TIMEOUT`, `COEFICIENTE_INVALIDO`, `MEDICAO_INVALIDA` |
| Armazenamento | `FLASH_INICIALIZACAO`, `FLASH_INDISPONIVEL`, `FLASH_ABERTURA`, `FLASH_GRAVACAO`, `SEM_ENSAIO`, `FLASH_FORMATO`, `FLASH_LEITURA`, `FLASH_POSICAO` |
| Rádio | `LORA_INICIALIZACAO`, `LORA_INDISPONIVEL`, `LORA_JSON`, `LORA_ENVIO`, `LORA_TX_TIMEOUT` |
| Serialização | `JSON_MEMORIA` |

A descrição informa a causa observada. Falhas de sensor invalidam a tara; após conferir o sensor, envie TARA sem carga com novo ID, sem necessidade de reiniciar. Falha TX_DONE desabilita o rádio até reiniciar; a aquisição em andamento pode continuar. Falhas de gravação encerram a aquisição.

## Roteiro de teste manual

Siga **CALIBRACAO_E_TARA.md**: descubra a sessão, aguarde a tara inicial, configure o coeficiente, execute nova tara e confira prontidão antes de iniciar. Repita solicitações com o mesmo ID para validar deduplicação. Use IDs novos para consultas atualizadas ou novas tentativas depois de falha.

O novo bloqueio de calibração significa que um roteiro antigo que enviava INICIAR imediatamente após o boot agora será recusado. Um ensaio novo aceito continua substituindo a gravação anterior.
