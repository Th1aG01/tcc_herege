# Roteiro serial — V0.17

Monitor Serial: **115200 baud**, final de linha **Nova linha**. Cada JSON abaixo ocupa uma linha. Nenhuma interface Python é necessária. Os exemplos começam após um boot; substitua `0123456789ABCDEF` pela sessão real, `ID_DO_ENSAIO` pelo arquivo escolhido e `ID_DA_TRANSFERENCIA` pelo ID retornado. Use IDs de solicitação crescentes maiores que `maior_id` informado pelo ESP.

## 1. Descobrir a sessão e escolher o arquivo

```json
{"protocolo":2,"tipo":"comando","id":0,"comando":"IDENTIFICAR"}
```

Confira firmware 0.17, protocolo USB 2, formato_gravacao 3 e protocolo_lora_gravado 1.

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":1,"comando":"LISTAR_ENSAIOS","limite":4}
```

Aguarde o evento LISTA_ENSAIOS. Selecione um `ensaio_id` com `condicao: concluido`. Se precisar criar um ensaio, siga as etapas de calibração, tara e aquisição em COMANDOS_E_TESTES.md e continue aqui usando IDs de pedidos maiores. Não é necessário refazer calibração/tara para enviar arquivos salvos.

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":2,"comando":"CONSULTAR_ENSAIO","ensaio_id":"ID_DO_ENSAIO"}
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":3,"comando":"LER_AMOSTRAS","ensaio_id":"ID_DO_ENSAIO","inicio":1,"limite":4}
```

Anote os metadados, contagem, duração e amostras para comparar depois. O envio usa os valores gravados, não a calibração operacional atual.

## 2. Iniciar e consultar sem receptor compatível

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":4,"comando":"ENVIAR","ensaio_id":"ID_DO_ENSAIO"}
```

Resultado inicial: `tipo: resposta`, `fase: aceito`, `transferencia.ativa: true`, estado VALIDANDO. Copie `transferencia.id`.

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":5,"comando":"CONSULTAR_ENVIO","transferencia_id":"ID_DA_TRANSFERENCIA"}
```

O ESP prepara e envia INICIO. TRANSFERENCIA_INICIADA confirma TX_DONE local. O amarelo acende apenas durante cada TX e apaga ao esperar ACK. Sem receptor BT/1, haverá até quatro tentativas de INICIO e então ENVIO_SEM_CONFIRMACAO, `fase: falhou`, `envio_local_concluido: false`, `recebimento_confirmado: false`. Isso é esperado; não indica que o arquivo foi apagado. O prazo inclui preparo e tempo de rádio, além das esperas de 2 segundos.

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":6,"comando":"CONSULTAR_ENVIO"}
```

Use ID novo a cada consulta. Repetir ID 5 devolve o retrato antigo, pela deduplicação. Se surgir LORA_TX_TIMEOUT, a falha é de TX_DONE local: conferir rádio/DIO0 na bancada; o firmware desabilita rádio até reset. Sem LoRa detectado, ENVIAR é rejeitado com LORA_INDISPONIVEL antes de aceitar.

## 3. Repetição do pedido e cancelamento

Inicie outra transferência:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":7,"comando":"ENVIAR","ensaio_id":"ID_DO_ENSAIO"}
```

Copie o **novo** ID de transferência. Reenvie a linha de ID 7 exatamente: deve retornar a aceitação original, com o mesmo ID de transferência, sem reiniciar ou abrir outra operação. Após término, essa repetição pode reapresentar também o evento terminal. Ela não é uma nova tentativa de envio: para isso, use novo ID de pedido.

Cancele antes do timeout:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":8,"comando":"CANCELAR","transferencia_id":"ID_DA_TRANSFERENCIA"}
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":9,"comando":"CONSULTAR_ENVIO"}
```

Esperado: evento ENVIO_CANCELADO no ID original 7, causado_por 8; resposta concluída do CANCELAR; arquivo fechado, amarelo apagado e nenhuma transmissão pendente local. Se já terminou por timeout, CANCELAR não altera o resultado anterior. Cancelamento remoto não é confirmado: o receptor futuro deverá expirar sua sessão parcial.

## 4. Conferir que o original permanece

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":10,"comando":"CONSULTAR_ENSAIO","ensaio_id":"ID_DO_ENSAIO"}
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":11,"comando":"LER_AMOSTRAS","ensaio_id":"ID_DO_ENSAIO","inicio":1,"limite":4}
```

Compare com as consultas anteriores. Nome, perfil, coeficiente, tara, duração, quantidade e valores devem ser iguais. Transmissão, falha ou cancelamento não excluem o ensaio. Reinicie e liste novamente para verificar a preservação física; a sessão USB mudará, os IDs persistentes dos arquivos não.

## 5. Verificar rejeições e proteções

Em uma nova transferência ativa, com novos IDs de pedido:

- ENVIAR sem ensaio_id: ENSAIO_ID_OBRIGATORIO quando o rádio/estado permitem iniciar; nunca seleciona um arquivo implicitamente.
- CANCELAR/CONSULTAR_ENVIO com transferencia_id de outra operação: TRANSFERENCIA_NAO_ENCONTRADA; não cancela a atual.
- INICIAR durante envio: recusado por envio ativo; a aquisição não começa.
- LER_AMOSTRAS do arquivo transmitido: ENSAIO_PROTEGIDO. CONSULTAR_ENSAIO pode consultar os metadados.
- EXCLUIR_ENSAIOS do arquivo transmitido: resultado individual ENSAIO_PROTEGIDO. Para testar isso, use um registro descartável e confirme primeiro que a transferência ainda está ativa; se ela já terminou, a exclusão será executada normalmente.
- ENVIAR durante aquisição: ENSAIO_ATIVO; gravação continua.
- ENVIAR imediatamente depois de FINALIZAR, com pacote ao vivo ainda em TX: RADIO_OCUPADO. Aguarde e tente com ID novo.

Não executar EXCLUIR_TODOS apenas para testar envio: é uma exclusão real dos ensaios não protegidos.

## 6. Reconectar

Sem reset: IDENTIFICAR (ID 0) → confira a mesma sessão → CONSULTAR_ENVIO com ID maior que maior_id. O estado em RAM continua, mesmo se mensagens intermediárias foram perdidas pela USB. O envio não depende do computador conectado.

Com reset: IDENTIFICAR retorna nova sessão e CONSULTAR_ENVIO mostra NENHUMA. Liste os arquivos, escolha novamente e solicite ENVIAR explicitamente. Nenhuma transmissão ou exclusão é retomada por conta própria. Abrir o Monitor Serial pode acionar reset por DTR/RTS; confira a sessão para distinguir os casos.

## 7. Quando o receptor BT/1 estiver implementado

Repetir o passo 2. Esperar blocos ACK avançando e TRANSFERENCIA_PROGRESSO. ENVIO_LOCAL_CONCLUIDO terá `envio_local_concluido: true` e `recebimento_confirmado: false`; é intermediário. Depois de o receptor validar e salvar o arquivo, deve chegar ENVIO_CONFIRMADO com `recebimento_confirmado: true`.

Se o receptor confirmar blocos mas não enviar CONFIRMAR final, o ESP repetirá FIM e terminará ENVIO_SEM_CONFIRMACAO mantendo `envio_local_concluido: true`. Não considerar isso recebimento integral comprovado.

Verificar com o receptor: ACK perdido, DADOS repetido, PEDIR de bloco inicial e final, confirmação final perdida, CRC incorreto, cancelamento, reboot e comparação integral dos arquivos. Essas verificações **ainda dependem do receptor e do hardware**. PROTOCOLO_LORA.md contém o contrato completo para implementá-las depois.
