# Protocolo USB 2 — firmware V0.15

O protocolo de **comandos** permanece 2. IDENTIFICAR anuncia também `formato_amostra: 2` e `formato_gravacao: 2`. A V0.15 altera o corpo das amostras e o arquivo: `ensaio_id` passa a ser uma string própria, distinta do ID numérico do pedido. O LoRa usa esse mesmo formato de amostra. A futura integração deve conferir a versão do firmware e esses formatos.

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
| `INICIAR` | `aceito` | Prepara nova gravação sem LoRa; confirmação real em AQUISICAO_INICIADA |
| `INICIAR_LORA` | `aceito` | Prepara gravação com LoRa; confirmação real em AQUISICAO_INICIADA |
| `FINALIZAR` | `concluido` | Encerra aquisição; se já ociosa, confirma sem iniciar outra ação |
| `LORA_LIGAR` | `concluido` | Habilita envio ao vivo durante aquisição |
| `LORA_DESLIGAR` | `concluido` | Desabilita próximos envios ao vivo; aquisição continua |
| `LORA_ALTERNAR` | `concluido` | Alterna o envio ao vivo; repetição do mesmo ID não alterna de novo |
| `ENVIAR` | `aceito` | Inicia envio do arquivo gravado; exige fechamento normal válido no formato 2 |
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
{"protocolo":2,"tipo":"resposta","sessao":"0123456789ABCDEF","ms":5000,"id":2,"comando":"INICIAR","ok":true,"fase":"aceito","estado":"PREPARANDO","lora_ao_vivo":false,"ensaio":{"id":"0123456789ABCDEF-1","solicitacao_id":2,"resultado":"PREPARANDO"}}
```

`aceito` indica que a operação prolongada foi aceita e preparada. **Não confirma a primeira amostra nem o término.** Para aquisição, aguarde AQUISICAO_INICIADA; para tara, TARA_CONCLUIDA; para envio gravado, ENVIO_CONCLUIDO. Os exemplos de controle omitem campos adicionais de estado/calibração para facilitar a leitura. Para comandos instantâneos e consultas, a fase é `concluido`. Não se envia uma resposta de sucesso quando uma ação é recusada.

### Amostra

```json
{"protocolo":2,"tipo":"amostra","ensaio_id":"0123456789ABCDEF-1","indice":1,"tempo_ms":0,"empuxo_kgf":1.5}
```

`ensaio_id` é uma string composta pela sessão aleatória do boot e um contador crescente de ensaios. É diferente do `id` numérico da solicitação. O contador começa em 1 e não se repete dentro da sessão; o prefixo distingue reinicializações com probabilidade extremamente alta, sem promessa de unicidade criptográfica global. A identidade física permanece disponível em IDENTIFICAR.

`indice` começa em 1 e avança por amostra gravada; `tempo_ms` começa em 0 na primeira leitura válida gravada. `empuxo_kgf` já contém a tara e a calibração, sem precisar de outra correção no computador. Somente amostras gravadas com sucesso são publicadas. Não há saída de tempo/leitura bruta em texto ou CSV.

USB e LoRa usam a mesma função de serialização e os mesmos campos. A estrutura binária conserva esses valores e o coeficiente do ensaio. A representação decimal segue a precisão do ArduinoJson; não altera a unidade ou refaz a calibração.

Quando a fila USB enche, amostras de telemetria podem ser descartadas para reservar espaço a controle. Isso não descarta a amostra já gravada em flash. Lacunas de `indice` e `usb.amostras_descartadas` tornam essa situação observável. Não há garantia de entrega de todas as amostras pela USB sob consumidor lento.

### Eventos de estado e conclusão

Eventos informativos têm `codigo`, `descricao` e `estado`, por exemplo `BOOT`, `USB_PRONTA`, `TARA_CONCLUIDA` e `ESTADO_ATUALIZADO`.

A primeira amostra gravada gera AQUISICAO_INICIADA, fase iniciado, com `id` do pedido inicial e objeto `ensaio`. PREPARANDO não acende o LED verde; ADQUIRINDO acende. Tara e alterações de coeficiente são bloqueadas em ambos.

Ao encerrar uma operação prolongada, o evento inclui o ID original, a fase terminal e contadores:

```json
{"protocolo":2,"tipo":"evento","sessao":"0123456789ABCDEF","ms":9000,"id":2,"causado_por":3,"codigo":"ENSAIO_ENCERRADO","fase":"concluido","descricao":"COMANDO_FINALIZAR","estado":"OCIOSO","amostras_gravadas":40,"amostras_enviadas":0,"ensaio":{"id":"0123456789ABCDEF-1","solicitacao_id":2,"resultado":"CONCLUIDO","motivo":"COMANDO_FINALIZAR","duracao_ms":4000,"amostras":40,"fechamento_confirmado":true}}
```

Neste exemplo, o comando de ID 3 (`FINALIZAR`) encerrou a operação de ID 2. A resposta do comando 3 é outra mensagem. O evento terminal pode aparecer antes da resposta do comando de parada/cancelamento.

| Código terminal | Fases possíveis |
| --- | --- |
| `ENSAIO_ENCERRADO` | `concluido` após comando, pelo menos uma amostra e fechamento confirmado; `falhou` por sensor, cálculo, espaço, gravação, fechamento ou parada antes da primeira amostra |
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

Erros contêm `codigo`, `origem` e `descricao`. `fase` é `rejeitado` para recusas de comando e `falha` para falhas assíncronas ou durante o fechamento já iniciado. Uma falha de fechamento pode ter o ID do comando FINALIZAR; o evento terminal correspondente conserva o ID de INICIAR. Erros de enquadramento/JSON não tentam extrair um ID de uma linha inválida.

Avisos usam `tipo: aviso`, `nivel: AVISO`, origem, código e descrição. O aviso `COEFICIENTE_PADRAO` informa que o valor inicial não foi explicitamente configurado e não libera o ensaio.

Em falta de memória para serializar, a mensagem fixa de emergência `JSON_MEMORIA` pode omitir sessão, tempo, fase e nível. Nenhum sistema de logs consegue assegurar entrega com falta de RAM ou transporte indisponível.

### Consulta de estado

| Grupo | Conteúdo |
| --- | --- |
| Identificação | Dispositivo, firmware, protocolo e sessão |
| `aquisicao` | `ativa` indica primeira amostra já gravada; `preparando` indica espera pela primeira. `id` é o pedido inicial enquanto pendente, ou 0; `amostras` é o contador em RAM |
| `ensaio` | ID próprio, solicitação original, coeficiente usado, início, resultado, motivo, duração, amostras e confirmação de fechamento |
| `sensor` | Leituras recentes, tara pronta e tara inicial em andamento |
| `calibracao` | kgf, coeficiente, origem padrão/USB, configuração explícita, validade e prontidão; validação física a cargo do operador |
| `tara` | Pronta/em andamento, progresso e timeout |
| `armazenamento` | Inicialização, LittleFS interno, formato 2 e limite de um ensaio |
| `radio` | Inicialização, TX, modo ao vivo, envio gravado, ID e progresso |
| `usb` | Maior ID consumido, amostras USB descartadas e mensagens de controle descartadas |

`OCIOSO` não significa que todos os componentes estão disponíveis. `ensaio` representa o ensaio atual ou o último resultado e permanece depois de FINALIZAR. Seu campo `coeficiente` é o valor usado no registro, não necessariamente a calibração atual. A duração em andamento é desde a primeira medição; após o término permanece fixa.

No boot, um fechamento válido recupera resultado, motivo, coeficiente, duração e contagem. Sem fechamento válido, resultado INCOMPLETO e contagem/duração/início null; não são estimados como se fossem confirmados. A solicitação original não é recuperada e aparece como 0. Não ocorre retomada automática. Estatísticas USB e rádio continuam sendo da sessão atual. `radio.pronto` não comprova recepção remota; a validade numérica da calibração não comprova exatidão física.

Use IDENTIFICAR e depois STATUS com novo ID para consultar um ensaio após reconectar, sem reenviar INICIAR. O roteiro completo e a diferença entre reconexão e reset estão em CICLO_AQUISICAO.md.

## Códigos de erro

| Grupo | Códigos |
| --- | --- |
| Enquadramento | `RX_TIMEOUT`, `RX_TAMANHO`, `RX_CARACTERE` |
| Envelope | `JSON_INVALIDO`, `ID_INVALIDO`, `COMANDO_INVALIDO`, `PROTOCOLO_INVALIDO`, `TIPO_INVALIDO`, `CAMPO_DESCONHECIDO` |
| Sessão e repetição | `SESSAO_INVALIDA`, `ID_CONFLITO`, `ID_EXPIRADO`, `REQUISICOES_OCUPADAS` |
| Operação | `COMANDO_DESCONHECIDO`, `ENSAIO_ATIVO`, `ENSAIO_INATIVO`, `ENVIO_ATIVO`, `ENSAIO_ID_ESGOTADO`, `LIMITE_AMOSTRAS` |
| Sensor/calibração | `HX711_TARA_TIMEOUT`, `HX711_TARA_INICIAL_TIMEOUT`, `TARA_NAO_PRONTA`, `TARA_EM_ANDAMENTO`, `CALIBRACAO_NAO_CONFIGURADA`, `HX711_INDISPONIVEL`, `HX711_TIMEOUT`, `COEFICIENTE_INVALIDO`, `MEDICAO_INVALIDA` |
| Armazenamento | `FLASH_INICIALIZACAO`, `FLASH_INDISPONIVEL`, `FLASH_ABERTURA`, `FLASH_GRAVACAO`, `SEM_ENSAIO`, `FLASH_LEITURA`, `FLASH_POSICAO`, `FLASH_SEM_ESPACO`, `FLASH_CABECALHO`, `FLASH_FECHAMENTO`, `FLASH_REGISTRO_INCOMPLETO`, `FLASH_ENSAIO_NAO_CONCLUIDO`, `FLASH_AMOSTRA_INVALIDA` |
| Rádio | `LORA_INICIALIZACAO`, `LORA_INDISPONIVEL`, `LORA_JSON`, `LORA_ENVIO`, `LORA_TX_TIMEOUT` |
| Serialização | `JSON_MEMORIA` |

A descrição informa a causa observada. Falhas de sensor invalidam a tara; após conferir o sensor, envie TARA sem carga com novo ID, sem necessidade de reiniciar. Falha TX_DONE desabilita o rádio até reiniciar; a aquisição em andamento pode continuar. Falhas de gravação encerram a aquisição.

## Roteiro de teste manual

Siga **CALIBRACAO_E_TARA.md**: descubra a sessão, aguarde a tara inicial, configure o coeficiente, execute nova tara e confira prontidão antes de iniciar. Repita solicitações com o mesmo ID para validar deduplicação. Use IDs novos para consultas atualizadas ou novas tentativas depois de falha.

INICIAR imediatamente após o boot é recusado até configurar calibração e concluir tara. Abertura de uma nova gravação substitui o arquivo anterior. Veja CICLO_AQUISICAO.md para início real, encerramento e reconexão; LEIA_ME.md informa as mudanças de formato.
