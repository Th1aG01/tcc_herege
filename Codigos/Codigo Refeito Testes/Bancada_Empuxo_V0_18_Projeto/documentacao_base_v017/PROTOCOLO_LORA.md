# LoRa BT/1 — contrato para o futuro receptor

Este documento especifica o transporte de um **arquivo salvo escolhido por ID**. Só o ESP da bancada foi alterado. O receptor ainda precisará implementar este contrato; os testes usam um receptor simulado, não firmware receptor entregue.

## Rádio preservado e tipos de tráfego

| Item | Configuração do arquivo base |
| --- | --- |
| CS / RST / DIO0 | GPIO 21 / 14 / 26 |
| SPI SCK / MISO / MOSI | GPIO 18 / 19 / 23 |
| Clock SPI | 1 MHz |
| Frequência | 915 MHz |
| SyncWord | 145 decimal (0x91) |
| Potência TX | 2 dBm |
| SF, BW, CR, preâmbulo e CRC do rádio | Mantidos como na inicialização da biblioteca/base; não foram adicionadas mudanças |
| LED verde / amarelo | GPIO 4 / 2 |

O receptor precisa usar parâmetros físicos compatíveis. BT/1 tem CRC32 próprio independentemente do CRC físico do rádio. Não altera frequência nem outros parâmetros para transmitir ACK.

Há dois formatos distintos:

- **Ao vivo:** pacote começa com `{`, contém JSON de amostra formato 2. Não tem ACK nem retransmissão; pode omitir amostras conforme disponibilidade do rádio. INICIAR_LORA/LORA_LIGAR controlam essa telemetria durante aquisição.
- **Arquivo salvo:** pacote começa com bytes ASCII `BT`, seguido da versão binária 1. Transporta o arquivo formato 3 integralmente, com verificação e confirmação. ENVIAR controla esse fluxo, somente fora da aquisição.

Não enviar BT/1 ao parser JSON do receptor. A serial USB da bancada continua exclusivamente JSON por linha; quadros BT/1 só circulam pelo rádio.

## Envelope binário

Todos os inteiros multibyte são **little-endian**. Não usar o layout nativo de uma struct para construir o envelope. IDs são ASCII com terminador NUL e preenchimento zero até 32 bytes; a comparação inclui todos os 32 bytes.

| Offset | Tamanho | Campo |
| ---: | ---: | --- |
| 0 | 2 | Magic: 0x42 0x54 (`BT`) |
| 2 | 1 | Versão: 1 |
| 3 | 1 | Tipo de quadro, tabela abaixo |
| 4 | 32 | ID da transferência |
| 36 | 32 | ID do ensaio |
| 68 | 4 | Sequência do quadro |
| 72 | 4 | K: quantidade total de blocos DADOS |
| 76 | 4 | Tamanho total do arquivo, em bytes |
| 80 | 4 | CRC32 do arquivo inteiro |
| 84 | 2 | N: tamanho do conteúdo deste quadro |
| 86 | N | Conteúdo, de 0 a 128 bytes |
| 86 + N | 4 | CRC32 dos primeiros 86 + N bytes do quadro |

Tamanho exato: **90 + N bytes**, máximo **218**, abaixo do limite de 255 do rádio. Não há LF nem texto hexadecimal adicional. Quadros de controle têm exatamente 90 bytes e N=0. O último DADOS pode ser menor; não acrescentar preenchimento ao seu conteúdo.

CRC32: polinômio refletido `0xEDB88320`, estado inicial `0xffffffff`, XOR final `0xffffffff` (CRC-32/ISO-HDLC). Vetor de referência: bytes ASCII `123456789` resultam em `0xCBF43926`. O CRC do arquivo cobre **todos os bytes**, inclusive seus CRCs internos. Enviar valores numéricos little-endian, não strings hexadecimais.

O ID de transferência combina a sessão de boot e `-T<contador>`, diferente do ID de pedido USB e do ID persistente do ensaio. Novo ENVIAR com novo ID de pedido produz outra transferência. Repetição idêntica do mesmo pedido USB não inicia outra.

## Tipos e sequências

| Valor | Nome | Direção | Sequência / conteúdo |
| ---: | --- | --- | --- |
| 1 | INICIO | Bancada → receptor | 0, sem conteúdo; identifica arquivo e transferência |
| 2 | DADOS | Bancada → receptor | 1 até K, conteúdo do arquivo |
| 3 | FIM | Bancada → receptor | K+1, sem conteúdo |
| 4 | ACK | Receptor → bancada | Sequência exata recebida, sem conteúdo |
| 5 | PEDIR | Receptor → bancada | Sequência solicitada novamente, sem conteúdo |
| 6 | CONFIRMAR | Receptor → bancada | K+1; confirma arquivo integral validado e salvo |

`K = ceil(tamanho_arquivo / 128)`. DADOS de sequência S contém os bytes a partir de `(S - 1) * 128`, até 128 bytes. Metadados, amostras e rodapé podem atravessar fronteiras de blocos. O receptor deve montar os bytes por posição, sem pressupor um pacote por amostra. Todos os quadros, inclusive ACK/PEDIR/CONFIRMAR, repetem os mesmos IDs, K, tamanho e CRC do arquivo.

## Fluxo e recuperação de perdas

1. ENVIAR abre o arquivo somente para leitura, confere cabeçalho/rodapé, valida uma amostra por passagem e calcula o CRC do arquivo em leituras de até 128 bytes por passagem. Só aceita encerramento normal do formato 3. Registros incompletos/legados continuam recuperáveis pelas operações USB que já os suportam; não entram nesta transferência normal.
2. A bancada envia INICIO e espera ACK de sequência 0. O receptor prepara uma sessão identificada por transferência/ensaio. INICIO repetido da mesma sessão **não pode zerar os blocos já guardados**; basta confirmar novamente.
3. A bancada envia um DADOS e espera seu ACK antes do seguinte. O receptor confere o envelope/CRC/posição, guarda o bloco e responde ACK. Duplicatas devem ser tratadas pela posição e receber novo ACK; não concatenar a mesma amostra duas vezes.
4. Depois de todos os DADOS terem ACK, a bancada transmite FIM. ENVIO_LOCAL_CONCLUIDO é emitido após esse TX_DONE. Isso ainda não comprova que o receptor validou o conjunto inteiro.
5. O receptor valida tamanho total, presença de todos os blocos, CRC do arquivo, estrutura do formato 3, identidade e CRCs internos; salva o arquivo integral. Só então envia CONFIRMAR de sequência K+1. A bancada conclui ENVIO_CONFIRMADO. ACK do FIM **não substitui CONFIRMAR**.
6. Se a confirmação se perder, a bancada repete FIM. O receptor deve reemitir CONFIRMAR para a mesma transferência já concluída, sem criar outra cópia por causa da repetição.

Durante a espera normal, PEDIR pode solicitar o quadro atualmente pendente. Durante a espera por confirmação do FIM, PEDIR pode solicitar qualquer sequência de 0 a K. A bancada reenvia esse quadro, aguarda seu ACK e então volta a enviar FIM. Solicite **um reparo por vez**. Não há lista de sequências, janela deslizante ou fila de pedidos. Para vários faltantes, repita o ciclo PEDIR → bloco/ACK → FIM. Não tente pedir blocos futuros durante o fluxo normal: são ignorados.

Quadro inválido não deve receber ACK. O timeout provoca nova tentativa. Em caso de arquivo completo mas CRC final incorreto, o receptor deve localizar/requisitar blocos suspeitos ou revalidá-los; não confirmar dados inválidos. Se não conseguir reparar dentro dos limites, encerre como incompleto no receptor.

## Prazos e limites

| Limite | Regra |
| --- | --- |
| Espera ACK/CONFIRMAR | 2000 ms após TX_DONE local, não após início de TX |
| Tentativas | Até 4 emissões do quadro pendente por ciclo: inicial + 3 repetições |
| Repetições/reparos globais | Até 64 reagendamentos por timeout/PEDIR na transferência inteira |
| Duração total | 2 horas, incluindo preparação |
| Espaçamento mínimo | 250 ms entre inícios de TX; não altera aquisição |
| TX sem TX_DONE | 5000 ms; rádio desabilitado até reiniciar |

O contador de tentativas reinicia ao avançar para outro quadro ou iniciar um reparo de outro quadro; o limite global continua valendo. `retransmissoes` conta os reagendamentos por timeout/PEDIR, não todo quadro FIM adicional depois de um reparo. Mensagens inválidas, ACKs antigos/futuros e mensagens de outra transferência são ignoradas **sem renovar o prazo**. Não existe espera infinita por receptor.

A bancada é half-duplex. Após TX_DONE desativa o callback/interrupt de TX e passa a consultar RX por `parsePacket()`. Antes do próximo TX restaura o callback. Isso evita que a ISR TX da biblioteca LoRa 0.8.0 consuma RX_DONE antes da consulta. Não há leitura de SPI/arquivo nem geração de JSON na ISR da aplicação.

O futuro receptor deve aguardar inicialmente **50 ms após receber um quadro** antes de responder, dando tempo para a bancada entrar em RX; esse intervalo é uma escolha inicial a verificar no hardware. A resposta inteira precisa terminar dentro dos 2000 ms da bancada. Medir esse retorno e os prazos com as configurações reais; nenhum teste nativo demonstra temporização RF.

## Estados, confirmação e cancelamento

CONSULTAR_ENVIO retorna `transferencia` com:

| Campo | Significado |
| --- | --- |
| id / ensaio_id / solicitacao_id | Transferência, arquivo e pedido ENVIAR |
| ativa / estado | Preparação, agendamento, TX, espera ou resultado terminal |
| amostras_total | Contagem do arquivo salvo |
| blocos_total / blocos_ack | K e quantidade de blocos consecutivos ACK |
| amostras_em_blocos_ack | Amostras cujos bytes inteiros já estão cobertos pelos blocos ACK; ainda não é confirmação integral |
| sequencia_atual / tentativas_atual | Quadro pendente e tentativas deste ciclo |
| quadros_tx | Quadros cujo TX foi iniciado, incluindo repetições; não significa TX_DONE ou recepção |
| retransmissoes / controles_ignorados | Contadores de reagendamento e controles recusados |
| bytes_arquivo / crc32_arquivo | Identificação do conteúdo; CRC só fica definido após preparação |
| envio_local_concluido | Primeiro FIM completou TX local; não é confirmação remota |
| recebimento_confirmado | Foi recebido CONFIRMAR válido nesta transferência |

Estados: NENHUMA, VALIDANDO, PREPARANDO, AGENDADA, TRANSMITINDO, AGUARDANDO_ACK, AGUARDANDO_CONFIRMACAO_FINAL; ao terminar, o estado é o código do resultado, por exemplo ENVIO_CONFIRMADO, ENVIO_SEM_CONFIRMACAO ou ENVIO_CANCELADO.

Eventos USB levam ID do ENVIAR, código, fase, origem, descrição e o retrato da transferência. Progresso é limitado a aproximadamente um evento por segundo, condicionado ao recebimento de ACK; não é um evento por amostra. Terminal fica retido no cache do pedido e o estado pode ser consultado, mesmo que a conexão USB tenha sido perdida.

CANCELAR atua **localmente**, inclusive abortando o TX atual, fecha o arquivo e apaga o amarelo. Não há quadro remoto de cancelamento nesta versão. O receptor precisa expirar sessões sem atividade (por exemplo, após 15 segundos sem quadros durante o fluxo) e manter arquivos parciais separados dos concluídos. A escolha de retenção dos parciais é do receptor. Uma sessão já confirmada deve continuar reconhecendo FIM repetido por pelo menos a janela de tentativas (recomenda-se 15 segundos), para reemitir CONFIRMAR.

Reset do ESP perde o estado da transferência e não a retoma. O arquivo original permanece no LittleFS. Ao reconectar, IDENTIFICAR e CONSULTAR_ENVIO permitem distinguir continuidade de reset. Nova transferência deve ser solicitada explicitamente; não há retomada de parcial de uma transferência antiga.

CRC e IDs verificam integridade e associação, não autenticam a estação. `recebimento_confirmado` significa que a bancada recebeu a declaração BT/1 válida do receptor; a validação e persistência remotas dependem da implementação dele.

## Operações simultâneas e LEDs

| Situação | Permitido / bloqueado |
| --- | --- |
| Durante aquisição | USB, gravação, telemetria ao vivo, consultas/catálogo e exclusão de outros arquivos; ENVIAR completo é recusado |
| Durante transferência completa | STATUS, CONSULTAR_ENVIO, catálogo/detalhes/espaço, CANCELAR e exclusão de outros arquivos |
| Nova aquisição durante transferência | Recusada; finalize/cancele o envio antes |
| Configurar coeficiente ou tarar durante transferência | Permitido se as demais regras de sensor/tara permitirem; não altera os valores guardados no arquivo transmitido |
| LER_AMOSTRAS/exclusão do arquivo transmitido | Recusados durante preparação, TX e espera de ACK/CONFIRMAR |
| EXCLUIR_TODOS durante transferência | Pode excluir outros arquivos; o arquivo protegido na aceitação permanece protegido durante toda a exclusão |
| ENVIAR imediatamente após FINALIZAR com último pacote ao vivo em TX | RADIO_OCUPADO; aguarde esse TX e use novo ID de solicitação |

O amarelo indica **TX local real**, inclusive repetições. Fica apagado em preparação/RX/espera e depois de fim, falha ou cancelamento. O verde indica aquisição real e permanece apagado na transferência de arquivo. Nenhum desligamento de LED representa, por si só, confirmação remota.

## Layout do arquivo formato 3 para o receptor

Inteiros uint32 little-endian, tara int32 little-endian e números double IEEE-754 de 64 bits. Strings UTF-8 terminadas em NUL dentro dos campos fixos. Os limites aceitos pela USB podem ser menores que a capacidade reservada no arquivo. CRC de cada registro cobre apenas seus bytes anteriores ao campo CRC.

| Cabeçalho: offset | Bytes | Campo |
| ---: | ---: | --- |
| 0 / 4 / 8 / 12 | 4 cada | Magic 0x42454E33 / formato 3 / tamanho cabeçalho 392 / tamanho amostra 64 |
| 16 | 32 | ID do ensaio |
| 48 | 8 | Coeficiente efetivamente utilizado |
| 56 | 4 | Tara interna, somente leitura |
| 60 | 4 | `kgf` seguido de NUL |
| 64 / 128 / 144 | 64 / 16 / 160 | Nome / data / observações |
| 304 / 344 | 40 / 40 | Identificação da célula / perfil |
| 384 / 388 | 4 / 4 | Reservado zero / CRC32 |

| Amostra: offset relativo | Bytes | Campo |
| ---: | ---: | --- |
| 0 / 4 | 4 / 4 | Tempo desde início em ms / índice desde 1 |
| 8 / 16 | 8 / 8 | Empuxo já calibrado em kgf / coeficiente utilizado |
| 24 | 32 | ID do ensaio |
| 56 / 60 | 4 / 4 | Reservado zero / CRC32 |

| Rodapé: offset relativo | Bytes | Campo |
| ---: | ---: | --- |
| 0 / 4 | 4 / 4 | Magic 0x46494D33 / resultado (1 normal, 2 falha) |
| 8 / 12 | 4 / 4 | Quantidade de amostras / duração em ms |
| 16 | 32 | Motivo do encerramento |
| 48 / 52 | 4 / 4 | Tamanho amostra 64 / CRC32 |

Tamanho esperado: `392 + quantidade * 64 + 56`. O receptor deve conferir ID do envelope/cabeçalho/amostras, índices, coeficiente coerente, valores finitos e unidade kgf. **Não aplicar novamente tara ou coeficiente ao empuxo armazenado.** O coeficiente e a tara são metadados de rastreabilidade, não correções a repetir.

## O que ainda depende do receptor e da bancada

Implementar o parser BT/1, armazenamento por posição, ACK/PEDIR/CONFIRMAR, deduplicação, expiração e validação integral no receptor. Depois, verificar perda de INICIO/ACK/DADOS/FIM/CONFIRMAR, reparo, pacote corrompido, sinal fraco, reinicialização de cada estação e comparação byte a byte do arquivo recebido com o original. Conferir latência RX/TX, DIO0, LEDs e temporização real. A simulação desta entrega cobre a lógica do ESP, mas não essas propriedades físicas.
