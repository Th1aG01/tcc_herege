# Ciclo de aquisição e testes pela serial — V0.15

## Preparação e início real

A requisição INICIAR exige armazenamento montado, espaço estimado para cabeçalho/amostra/rodapé, calibração configurada e válida, tara pronta, sensor com leitura nos últimos 2000 ms e ausência de outra aquisição ou envio gravado. INICIAR_LORA exige também rádio disponível. A aquisição não depende de um receptor LoRa nem de confirmação do computador.

Depois das validações, o firmware abre o único arquivo para substituição, grava o cabeçalho, atribui `ensaio.id` e responde `fase: aceito`, `estado: PREPARANDO`. O LED verde continua apagado. Tara e mudança de coeficiente ficam bloqueadas desde essa preparação até o encerramento.

A primeira leitura válida é calibrada e gravada. Só então o firmware emite `AQUISICAO_INICIADA` com o ID do pedido inicial e os dados do ensaio, passa a ADQUIRINDO e acende o LED verde. Essa primeira amostra tem `indice: 1` e `tempo_ms: 0`. Não basta receber a aceitação para considerar o ensaio iniciado. Se não chegar leitura, o timeout de sensor encerra a tentativa com falha. FINALIZAR antes da primeira amostra informa `CANCELADO_ANTES_INICIO`, fase `falhou`, sem apresentar ensaio vazio como concluído normalmente.

## Roteiro para copiar

Monitor Serial: **115200 baud**, final de linha **Nova linha**. Cada JSON abaixo deve ser enviado em uma única linha.

1. Descubra a sessão:

```json
{"protocolo":2,"tipo":"comando","id":0,"comando":"IDENTIFICAR"}
```

2. Nos próximos exemplos, substitua **0123456789ABCDEF** pela sessão retornada. Os IDs 1–7 pressupõem uma sessão nova: se já houve comandos, use IDs maiores que `maior_id`. Aguarde a tara inicial terminar ou falhar. Configure o coeficiente do seu perfil, em contagens/kgf:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":1,"comando":"CONFIGURAR_CALIBRACAO","coeficiente":1000.0}
```

**1000.0 é apenas exemplo de formato**, não o coeficiente medido para sua célula. Valores positivos ou negativos são aceitos; a calibração física deve fornecer o valor correto. Não envie offset ou unidade.

3. Sem carga, execute a tara e aguarde `TARA_CONCLUIDA`, `id: 2`:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":2,"comando":"TARA"}
```

4. Consulte a prontidão:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":3,"comando":"STATUS"}
```

Espere `sensor.respondendo: true`, `calibracao.pronta_para_uso: true`, `tara.pronta: true`, armazenamento pronto e nenhuma operação conflitante. O comando de início ainda confirma abertura e escrita do cabeçalho, que podem falhar mesmo com o sistema montado.

5. Prepare a aquisição:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":4,"comando":"INICIAR"}
```

A resposta de aceitação inclui o ID próprio do ensaio. Aguarde o evento `AQUISICAO_INICIADA`, associado ao pedido 4, e o LED verde. Uma amostra ilustrativa é:

```json
{"protocolo":2,"tipo":"amostra","ensaio_id":"0123456789ABCDEF-1","indice":1,"tempo_ms":0,"empuxo_kgf":1.5}
```

O número 1.5 só ilustra o formato; não é a leitura esperada sem carga. Faça pequenas cargas conhecidas para conferir sinal e escala. O índice cresce em cada amostra gravada, mesmo quando a USB não consegue exibi-la.

6. Consulte durante o ensaio:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":5,"comando":"STATUS"}
```

Confira `aquisicao.ativa: true`, `ensaio.id`, `ensaio.amostras`, `ensaio.duracao_ms` e `ensaio.resultado: ADQUIRINDO`. A duração conta desde a primeira medição, não desde a chegada do comando. Consultas não reiniciam o ensaio.

7. Finalize:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":6,"comando":"FINALIZAR"}
```

O LED verde apaga ao encerrar a aquisição. O evento `ENSAIO_ENCERRADO` mantém `id: 4`, acrescenta `causado_por: 6` e contém o objeto `ensaio`: duração, quantidade de amostras, motivo `COMANDO_FINALIZAR`, resultado `CONCLUIDO` e `fechamento_confirmado: true`. A fase terminal é `concluido` somente se houve pelo menos uma amostra e o fechamento foi verificado. A duração inclui o intervalo entre a primeira amostra e a parada, mas exclui o trabalho de fechamento da flash.

O evento terminal pode anteceder a resposta de FINALIZAR. Na falha de fechamento, FINALIZAR recebe erro `FLASH_FECHAMENTO`, `fase: falha`; o evento do pedido inicial também indica falha. Repetir FINALIZAR não grava outro rodapé.

8. Consulte o resultado:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":7,"comando":"STATUS"}
```

O estado será OCIOSO, com `aquisicao.ativa: false`, e `ensaio` conservará o último resumo até outro início ou reinicialização. A calibração atual é separada de `ensaio.coeficiente`, que identifica o coeficiente usado nesse registro.

## Reconectar sem reiniciar o ensaio

1. Mantenha a alimentação da placa. Feche apenas o Monitor Serial para simular ausência do computador. Retirar o cabo quando ele é a única fonte desliga o ESP e não testa funcionamento autônomo.
2. Reabra a mesma porta a 115200. Envie IDENTIFICAR com ID 0 e sem sessão.
3. Confira a sessão recebida. Se for a mesma, envie STATUS com essa sessão e um ID **maior que `maior_id`**. Não envie INICIAR para descobrir o estado.
4. STATUS informa o ID do ensaio, preparação/aquisição, solicitação original, contagem, duração e resultado. Se o ensaio ainda estiver ativo, a contagem terá continuado crescendo.
5. Se quiser parar, envie FINALIZAR com outro ID novo. Se quiser repetir um pedido cuja resposta se perdeu, preserve exatamente sua sessão, ID e parâmetros.

Alguns adaptadores/monitores alteram DTR/RTS e reiniciam o ESP ao abrir a porta. **Sessão diferente significa novo boot**, não uma simples reconexão. Nesse caso a aquisição foi interrompida pelo reset: STATUS mostra o registro recuperado como concluído/falho apenas se existe fechamento válido; sem rodapé válido, informa `INCOMPLETO`, com duração, contagem e início `null` por não terem sido recuperados. A solicitação original não persiste e aparece como 0. Não há retomada automática.

O firmware não espera ACK USB, não consulta presença de host para gravar e não interrompe o ensaio por fila USB cheia. Ele descarta mensagens de amostra inteiras quando necessário, preservando os registros já escritos. `usb.amostras_descartadas` e lacunas no índice mostram a perda de telemetria. A fila é finita: após reconectar podem chegar mensagens antigas ainda pendentes. O STATUS com ID novo é a referência atual; repetir o mesmo STATUS devolve a resposta antiga retida. Não foi acrescentado download histórico pela USB nesta etapa.

## USB, flash e LoRa

O firmware obtém uma leitura quando HX711 informa dado pronto, no máximo uma por passagem do loop. A conta é `(leitura bruta - tara interna) / coeficiente`, aplicando cada correção uma única vez. A mesma estrutura calibrada é gravada e usada para USB e rádio. Uma amostra com escrita incompleta não é publicada.

Não existe temporizador de aquisição de 250 ms. **250 ms é somente o intervalo mínimo de agendamento do rádio.** No modo ao vivo, o LoRa envia a última amostra ainda não transmitida quando está livre, podendo pular índices. Um rádio ocupado não impede a leitura/gravação; tampouco repete indefinidamente uma amostra antiga. Não se promete transmissão de todas as amostras ao vivo. A taxa efetiva de aquisição depende do HX711, do loop e da latência de flash/serial/SPI; precisa ser medida no hardware.

Use INICIAR_LORA em vez de INICIAR para habilitar rádio desde a preparação, ou LORA_LIGAR/LORA_DESLIGAR durante o ensaio. Depois de um encerramento normal, ENVIAR transmite todas as amostras gravadas em sequência, com os valores e IDs originais. Alterar a calibração atual depois do ensaio não recalcula o arquivo. O LED amarelo acompanha TX local; conclusão de envio não confirma recepção remota. Falha de rádio desabilita sua transmissão, mas não encerra a aquisição.

## Falhas a verificar

| Teste | Resultado esperado |
| --- | --- |
| INICIAR antes de configurar calibração | CALIBRACAO_NAO_CONFIGURADA, sem abrir novo arquivo |
| Configurar coeficiente e iniciar sem nova tara | TARA_NAO_PRONTA |
| TARA ou CONFIGURAR_CALIBRACAO durante PREPARANDO/ADQUIRINDO | ENSAIO_ATIVO, sem alteração |
| Repetir o mesmo pedido INICIAR | Mesma aceitação/ID, sem reiniciar contagem ou truncar o arquivo |
| Sensor deixa de fornecer leituras | HX711_TIMEOUT após 2000 ms, tara invalidada, LED verde apagado, ensaio FALHOU |
| Sem espaço para iniciar | FLASH_SEM_ESPACO, sem substituir a gravação anterior |
| Falha ao criar cabeçalho | FLASH_CABECALHO, sem confirmar aquisição |
| Falta de espaço durante aquisição | FLASH_SEM_ESPACO, tentativa de fechar como falho, sem conclusão normal |
| Escrita parcial | FLASH_GRAVACAO e eventual FLASH_FECHAMENTO; amostra parcial não entra na contagem/publicação |
| Falha no rodapé/flush/reabertura/verificação | FLASH_FECHAMENTO, fechamento não confirmado, ensaio FALHOU |
| ENVIAR registro incompleto/falho/antigo | FLASH_ENSAIO_NAO_CONCLUIDO |
| Rádio ocupado ou sem TX_DONE | Gravação continua; timeout desabilita rádio após 5000 ms |
| Fechar o monitor sem reset/desligamento | Aquisição continua; STATUS após reconectar identifica o mesmo ensaio |

Falhas de escrita/flush/reabertura foram injetadas nos testes nativos. Não há comando de diagnóstico que provoque essas falhas na placa. Não altere a pinagem para executar o roteiro. Testes elétricos, taxa real, desconexões e retenção de dados precisam ser realizados na bancada.

## Formato de gravação 2

Um arquivo `/ensaio.bin`, sem histórico interno: cabeçalho de 48 bytes, N amostras de 56 bytes, rodapé de 48 bytes. Tipos fixos uint32/double e arrays de 32 caracteres; tamanhos verificados com static_assert. Formato interno do ESP32 (little-endian), não um CSV nem um protocolo de transporte portátil.

- Cabeçalho: magic 0x42454E32, formato 2, ID do ensaio e coeficiente.
- Amostra: tempo uint32, índice uint32, empuxo double em kgf, coeficiente double e ID.
- Rodapé: magic 0x46494D32, resultado 1 (normal) ou 2 (falho/sem aquisição), quantidade, duração e motivo.

O firmware verifica cabeçalho, tamanho total, número de amostras e rodapé ao fechar/reabrir, no boot e antes de ENVIAR. No envio, cada amostra também é conferida quanto a índice, identidade, coeficiente e valor finito. O rodapé não é contado nem transmitido como amostra. Sem rodapé válido, o arquivo nunca é considerado normalmente concluído. Um rodapé válido de falha preserva contagem e motivo, mas também é recusado pelo envio normal.

`File.flush()` e `close()` não fornecem retorno de erro no core consultado. Por isso existe a releitura estrutural, que detecta falhas observáveis; ela **não é uma garantia de durabilidade contra queda de energia nem um teste completo de integridade física**. Não há CRC ou recuperação individual de registros nesta etapa. O resumo de arquivo incompleto após reset não presume quantas amostras foram preservadas.
