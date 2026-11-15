# Bancada de Empuxo V0.12 — correção de compilação LoRa

A chamada a `LoRa.isTransmitting()` era inválida porque esse método é privado. A responsabilidade por essa incompatibilidade foi do firmware entregue, não de uma configuração sua.

Esta entrega corrige somente essa incompatibilidade e retira a pasta `historico`. Os comandos seriais, a pinagem, a calibração e os parâmetros de rádio permanecem os da etapa anterior. A base é o último firmware entregue neste chat, V0.10; como o caminho informado no erro já usa V0.11, a correção foi numerada V0.12 para evitar confusão. Alterações particulares feitas em outra V0.11 não estão incluídas.

## Como carregar

1. Extraia o ZIP em uma nova pasta.
2. Abra `Bancada_Empuxo_V0_12/Bancada_Empuxo_V0_12.ino`.
3. Mantenha a placa, porta, partições e bibliotecas que você já utiliza. Não edite o arquivo LoRa.h nem torne métodos privados públicos.
4. Clique em Verificar na Arduino IDE; se compilar, faça o upload.
5. Abra o Monitor Serial em 115200 baud e selecione Nova linha.

## Correção

- Removidas as duas chamadas diretas a `isTransmitting()`.
- Registrado o callback público `LoRa.onTxDone(aoConcluirTransmissaoLoRa)`.
- Uma variável `volatile` indica o pacote em andamento. Ela é marcada antes de `endPacket(true)` e liberada no callback, inclusive se a conclusão ocorrer rapidamente.
- O callback apenas atualiza a variável; não escreve na serial nem acessa a memória.
- O LED amarelo e a conclusão do envio gravado consultam essa variável. O envio permanece assíncrono.

A notificação TX_DONE depende da conexão **DIO0 do rádio ao GPIO 26**, já definido no firmware original. Sem essa notificação, o firmware pode continuar aguardando o fim do pacote, com o amarelo aceso; confira essa ligação se isso ocorrer. A conclusão local não confirma recebimento na estação remota.

## Testes pela serial

Envie uma linha de cada vez:

| Comando | Resultado esperado |
| --- | --- |
| `STATUS` | Firmware 0.12 e estado dos componentes |
| `AJUDA` | Lista dos comandos |
| `INICIAR` | Aquisição, LED verde aceso e amostras JSON |
| `LORA_LIGAR` | Envio ao vivo; amarelo indica TX |
| `LORA_DESLIGAR` | Para os próximos envios ao vivo; aquisição continua |
| `FINALIZAR` | Encerra aquisição; verde apaga |
| `ENVIAR` | Transmite a gravação, depois emite ENVIO_CONCLUIDO |
| `CANCELAR` | Cancela próximos pacotes do envio gravado |

Aguarde alguns segundos entre iniciar e finalizar. Um novo ensaio continua substituindo a gravação anterior, como no firmware base. O coeficiente original 1.0 continua sem calibração física; não interprete os números como kgf calibrados. Esta correção não adiciona tara por comando, recebimento de coeficiente ou ignição.

`COMANDOS.md` documenta o protocolo completo. `VALIDACAO.md` separa verificações locais e testes pendentes na placa. `validacao_nativa` contém fontes e resultados dos testes opcionais, sem necessidade de copiá-los para a pasta do sketch. Nenhuma versão anterior acompanha este ZIP.
