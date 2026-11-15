# Protocolo serial — V0.12 / protocolo 1

## Entrada

Serial USB em 115200 baud. Envie um comando **de texto**, terminado por LF (`\n`), CR (`\r`) ou CRLF (`\r\n`). Não é uma entrada JSON nesta etapa. Maiúsculas e minúsculas são equivalentes; espaços ou tabulações no começo e no fim são ignorados. O campo de resposta `comando` repete o texto normalizado para maiúsculas, inclusive o atalho, quando houver.

Cada linha aceita até 63 caracteres antes do terminador. Linhas vazias são ignoradas. Linhas longas ou com caracteres de controle inválidos são descartadas inteiramente até o próximo terminador, com um erro. Um comando incompleto não bloqueia a aquisição: permanece no buffer até chegar o final de linha. Não há parâmetros, comandos separados por ponto e vírgula ou execução letra a letra. Por exemplo, `if` é recusado, não equivale a iniciar e finalizar.

## Comandos disponíveis

| Comando | Atalho | Ação / condição |
| --- | --- | --- |
| `STATUS` | `s` | Consulta estado, versão, coeficiente, componentes e contadores |
| `AJUDA` | `?` | Retorna a lista de comandos e o formato de entrada |
| `INICIAR` | `i` | Inicia aquisição e uma nova gravação, sem envio LoRa; exige memória, tara e leituras recentes |
| `INICIAR_LORA` | `a` | Inicia aquisição, gravação e LoRa ao vivo; também exige rádio inicializado |
| `FINALIZAR` | `f` | Encerra aquisição e fecha o arquivo; repetir sem ensaio ativo é aceito |
| `LORA_LIGAR` | — | Habilita LoRa ao vivo durante um ensaio; repetir não alterna nem reinicia o ensaio |
| `LORA_DESLIGAR` | — | Desabilita LoRa ao vivo, mantendo aquisição e gravação |
| `LORA_ALTERNAR` | `l` | Atalho antigo: alterna o envio LoRa ao vivo |
| `ENVIAR` | `e` | Transmite o ensaio gravado; exige aquisição encerrada, arquivo válido e rádio disponível |
| `CANCELAR` | `c` | Cancela os próximos pacotes do envio gravado; repetir sem envio ativo é aceito |

`FINALIZAR` não cancela um envio gravado; use `CANCELAR`. `CANCELAR` não encerra aquisição. Um pacote já entregue ao rádio pode terminar depois do cancelamento: o amarelo continua acompanhando esse pacote. Para automação futura, prefira os comandos explícitos de ligar/desligar ao atalho de alternância.

Não existem comandos de motor, coeficiente, offset, duração, exclusão ou tara nesta etapa. `TARA`, `INICIAR_MOTOR` e `COEFICIENTE 1000`, por exemplo, retornam `COMANDO_DESCONHECIDO` sem executar ações.

## Saída

Cada linha da aplicação é um objeto JSON compacto, seguido de final de linha. Não há cabeçalho CSV nem texto livre intercalado pela aplicação. Mensagens da ROM, do bootloader ou mensagens de depuração do core ESP32 podem aparecer durante a reinicialização; não pertencem a este protocolo.

| `tipo` | Significado |
| --- | --- |
| `resposta` | Comando de ação aceito, com `ok: true` e estado resultante |
| `status` | Resultado da consulta `STATUS`, ou estado emitido no fim do boot |
| `ajuda` | Resultado de `AJUDA` |
| `amostra` | Uma amostra gravada com sucesso, com empuxo calculado em kgf |
| `erro` | Comando recusado ou falha detectada, com `ok: false`, código, nível, origem e mensagem |
| `aviso` | Condição que precisa ser conhecida, como o coeficiente original 1.0 |
| `evento` | Boot ou mudança de estado, como final de ensaio ou envio |

`protocolo` vale 1. Nas mensagens de controle, `ms` é o tempo desde o boot, e não um horário de calendário; no ESP32 ele volta a zero após o limite do contador de 32 bits. Nas amostras, `tempo_ms` é o tempo desde o início do ensaio. `indice` começa em 1 e permite observar lacunas na saída serial. As amostras omitem os campos repetidos de configuração para reduzir tráfego; unidade e coeficiente podem ser consultados em `STATUS`.

O campo `comando` aparece quando a mensagem foi gerada durante o processamento de uma linha válida. Eventos espontâneos, erros detectados no loop e erros de enquadramento podem não ter esse campo. Um comando de ação aceito produz uma `resposta`; um comando recusado produz um `erro`, sem confirmação de sucesso. Eventos adicionais podem preceder a resposta, como no encerramento do ensaio.

Exemplos ilustrativos; os tempos, contadores e valores reais variam:

```text
INICIAR
```

```json
{"protocolo":1,"tipo":"resposta","ms":5000,"comando":"INICIAR","ok":true,"estado":"ADQUIRINDO","lora_ao_vivo":false,"amostras_gravadas":0,"amostras_enviadas":0}
{"protocolo":1,"tipo":"amostra","tempo_ms":100,"empuxo_kgf":1.5,"indice":1}
```

O exemplo de 1.5 kgf pressupõe um coeficiente calibrado; não é uma leitura produzida ou validada com o hardware do usuário.

```text
XYZ
```

```json
{"protocolo":1,"tipo":"erro","ms":5100,"comando":"XYZ","ok":false,"nivel":"ERRO","origem":"Serial","codigo":"COMANDO_DESCONHECIDO","mensagem":"Comando nao reconhecido. Use AJUDA."}
```

O evento `PRONTO` apenas indica que a inicialização terminou. `STATUS` deve ser usado para avaliar `tara_pronta`, `hx711_respondendo`, `memoria_pronta` e `lora_pronto`. `OCIOSO` significa ausência de aquisição/envio gravado; não significa ausência de falhas.

Os contadores são da sessão de firmware. Após reiniciar, `amostras_gravadas` começa em zero mesmo que `/ensaio.bin` exista. `ENVIAR` descobre o total do arquivo e o informa em `amostras_para_enviar` nas consultas seguintes. Não há enumeração de histórico nesta etapa.

`lora_pronto` indica inicialização local do módulo. `ENVIO_CONCLUIDO` confirma a conclusão local do envio dos pacotes; não há confirmação de recebimento, retransmissão por perda ou verificação de uma estação remota. No modo ao vivo, a última leitura é enviada a cada 250 ms; no modo gravado, cada amostra do arquivo é enviada nessa cadência. Um rádio ocupado adia o pacote sem avançar a amostra do arquivo.

## Teste LoRa opcional, comandado pela serial

Depois de validar a aquisição sem rádio:

1. `INICIAR` — inicie e espere aparecerem amostras.
2. `LORA_LIGAR` — amarelo passa a indicar os períodos de transmissão; verde permanece aceso.
3. `LORA_DESLIGAR` — amarelo apaga após o pacote em andamento; aquisição continua.
4. `FINALIZAR` — encerre a gravação.
5. `ENVIAR` — transmite o arquivo, com o verde apagado e amarelo durante TX.
6. `STATUS` — consulte o progresso. Para interromper os próximos pacotes, use `CANCELAR`.

Também é possível iniciar diretamente com `INICIAR_LORA`. A comprovação da chegada dos pacotes depende do seu receptor LoRa. A serial permanece como canal de comandos; nenhum comando é recebido por LoRa nesta entrega.

## Erros e aviso

| Código | Origem / situação |
| --- | --- |
| `COMANDO_DESCONHECIDO` | Serial: linha não corresponde a um comando completo |
| `COMANDO_LONGO` | Serial: linha excede 63 caracteres; descarte até o terminador |
| `COMANDO_CARACTERE` | Serial: controle inválido ou caractere fora do ASCII permitido |
| `ENSAIO_ATIVO` | ESP32: operação incompatível com aquisição em andamento |
| `ENSAIO_INATIVO` | ESP32: tentativa de ligar LoRa ao vivo sem aquisição |
| `ENVIO_ATIVO` | ESP32: operação incompatível com envio gravado em andamento |
| `HX711_TARA_TIMEOUT` | HX711: não chegaram dez leituras dentro de três segundos no boot |
| `HX711_INDISPONIVEL` | HX711: início recusado por ausência de tara válida ou leituras recentes |
| `HX711_TIMEOUT` | HX711: nenhuma leitura nova por dois segundos; encerra aquisição e invalida a tara |
| `COEFICIENTE_INVALIDO` | ESP32: constante de calibração zero, NaN ou infinita |
| `MEDICAO_INVALIDA` | HX711: resultado do cálculo não finito; interrompe a aquisição |
| `FLASH_INICIALIZACAO` | Flash: falha de montagem no boot |
| `FLASH_INDISPONIVEL` | Flash: operação recusada porque a montagem falhou |
| `FLASH_ABERTURA` | Flash: falha ao criar a gravação |
| `FLASH_GRAVACAO` | Flash: escrita incompleta, inclusive por possível falta de espaço; encerra aquisição |
| `SEM_ENSAIO` | Flash: arquivo inexistente, vazio ou não aberto para leitura |
| `FLASH_FORMATO` | Flash: tamanho do arquivo não é múltiplo do registro `Payload` |
| `FLASH_LEITURA` | Flash: amostra esperada não foi lida completamente |
| `FLASH_POSICAO` | Flash: reposicionamento para repetir envio falhou |
| `LORA_INICIALIZACAO` | LoRa: falha de inicialização no boot |
| `LORA_INDISPONIVEL` | LoRa: operação solicitada sem rádio inicializado |
| `LORA_JSON` | LoRa: pacote JSON não cabe no buffer ou não pode ser montado |
| `LORA_ENVIO` | LoRa: falha ao preparar/iniciar o pacote; suspende envio ao vivo ou encerra envio gravado |
| `JSON_MEMORIA` | ESP32: memória insuficiente para montar uma mensagem serial |
| `COEFICIENTE_PADRAO` | **AVISO**, ESP32: coeficiente 1.0 original; não representa calibração validada |

Após uma falha de leitura do HX711, confira as conexões e reinicie sem carga para refazer a tara. O retorno de leituras sozinho não revalida a tara nem reinicia um ensaio.

A mensagem de emergência `JSON_MEMORIA` é um literal fixo, sem alocação dinâmica, e pode omitir `ms` e `comando`. Falta de RAM extrema ou falha do próprio transporte serial não permite garantir entrega de um log.

## Encaminhamento futuro ao log da interface

A Etapa 06 oferece `registrar_erro(origem, mensagem, detalhes="", nivel="ERRO")`. Uma integração posterior poderá usar `origem`, `mensagem` e `nivel` do firmware, colocando `codigo`, `comando` e `ms` em detalhes. O computador deverá registrar sua própria data e hora de recebimento. Essa adaptação não está implementada neste ZIP.

O firmware informa falhas que consegue observar; ele não mede continuidade do ignitor, precisão da célula, qualidade da alimentação ou recepção remota. Não existe histórico persistente de erros no ESP nesta etapa: copie a saída do Monitor Serial para guardar uma sessão.
