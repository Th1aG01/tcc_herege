# Protocolo USB 2 — firmware V0.16

## Transporte e identidade

USB/UART 115200 baud, 8N1. Um objeto JSON por linha, terminado por LF; CRLF também aceito. Limite 512 bytes antes de LF, incluindo espaços e CR. Mensagens fragmentadas são acumuladas sem executar antes do LF. Inatividade de recepção de 1000 ms descarta a linha parcial até o próximo LF. Tamanho excessivo/controle inválido também descarta a linha inteira. No máximo 64 bytes e uma linha processados por passagem do loop.

A saída usa fila de 8192 bytes, respostas de até 1535 bytes e escrita de até 64 bytes por passagem, conforme espaço na UART. Amostras USB podem ser descartadas para reservar espaço aos controles; a gravação não depende de ACK nem da presença do computador. O catálogo também aguarda espaço para emitir resultados. O firmware da aplicação produz JSON; ROM, bootloader e mensagens de depuração do core/bibliotecas não fazem parte desse protocolo.

Descoberta especial, repetível e sem efeitos:

```json
{"protocolo":2,"tipo":"comando","id":0,"comando":"IDENTIFICAR"}
```

Retorna dispositivo, firmware 0.16, sessão do boot, protocolo 2, baud 115200, maior ID de pedido consumido, limites de recepção, `formato_amostra: 2` e `formato_gravacao: 3`.

Comandos normais:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":1,"comando":"STATUS"}
```

Substitua a sessão pelo valor real. IDs de pedidos são inteiros 1–4294967295, novos em ordem crescente por sessão. `ensaio_id` é uma string independente: identifica o arquivo persistente. O nome do comando usa A–Z e `_`, até 24 caracteres. Campos não previstos são rejeitados; não são ignorados.

## Comandos

| Comando | Parâmetros além do envelope | Resultado |
| --- | --- | --- |
| IDENTIFICAR | Nenhum | Identidade, sessão, versões e limites |
| STATUS | Nenhum | Aquisição, sensor, calibração, tara, armazenamento, rádio, USB e operação de catálogo |
| AJUDA | Nenhum | Lista de comandos |
| CONFIGURAR_CALIBRACAO | coeficiente numérico, obrigatório | Fator finito e não zero em contagens/kgf; invalida tara |
| CONSULTAR_CALIBRACAO | Nenhum | Estado operacional da calibração e tara |
| TARA | Nenhum | Aceitação e evento terminal; dez leituras brutas, timeout 3000 ms |
| INICIAR / INICIAR_LORA | nome, data, observacoes, celula_id, perfil_id opcionais | Cria arquivo novo; aceitação e confirmação real após primeira amostra |
| FINALIZAR | Nenhum | Encerra gravação; evento terminal vinculado ao pedido de início |
| LORA_LIGAR / LORA_DESLIGAR / LORA_ALTERNAR | Nenhum | Controla LoRa ao vivo, sem controlar a aquisição |
| ENVIAR | ensaio_id recomendado; compatibilidade sem ID descrita abaixo | Envia registro concluído do formato 3 pelo LoRa |
| CANCELAR | Nenhum | Interrompe agendamento do envio gravado; pacote em TX pode terminar |
| LISTAR_ENSAIOS | limite 1–4, apos, revisao | Aceitação e evento LISTA_ENSAIOS com uma página |
| CONSULTAR_ENSAIO | ensaio_id obrigatório | Metadados e condição de um arquivo |
| LER_AMOSTRAS | ensaio_id obrigatório; inicio ≥1 e limite 1–4 opcionais | Bloco identificado; padrões inicio 1, limite 4 |
| EXCLUIR_ENSAIOS | ensaio_ids: array de 1–4 IDs distintos | Uma resposta com resultado de cada exclusão |
| EXCLUIR_TODOS | confirmacao: "EXCLUIR_TODOS" obrigatório | Aceitação, resultado por arquivo e evento terminal |
| ESPACO | Nenhum | Bytes totais, usados, livres e reserva |

Não existe comando de motor, formatação, offset externo ou seleção de unidade. Todos os comandos chegam pela serial USB. FINALIZAR e CANCELAR continuam sendo operações diferentes.

ENVIAR sem ID conserva o atalho para o último ensaio conhecido em RAM nesta inicialização; após reset não escolhe automaticamente um arquivo. Para múltiplos ensaios, forneça sempre ensaio_id. O LoRa é transmissão local sem ACK do receptor.

## Metadados de início

| Campo | Limite do texto decodificado | Regra |
| --- | --- | --- |
| nome | 48 bytes UTF-8 | Opcional, padrão vazio |
| data | 10 bytes | Vazio ou YYYY-MM-DD válido, inclusive regra de ano bissexto |
| observacoes | 128 bytes UTF-8 | Opcional; aceita `\n`, `\r`, `\t` escapados |
| celula_id | 32 bytes UTF-8 | Identificação fornecida pelo computador |
| perfil_id | 32 bytes UTF-8 | Identificação fornecida pelo computador |

O limite de 512 bytes continua valendo para o JSON completo, inclusive escapes. Caracteres com acentos podem ocupar mais de um byte. NUL, UTF-8 inválido, tipos errados, controles não permitidos e textos acima do limite são rejeitados. Não há truncamento silencioso. Esses campos não são usados para formar nomes de arquivo.

Coeficiente e referência interna de tara são obtidos do estado operacional e gravados no cabeçalho. Não envie coeficiente dentro de INICIAR: configure-o antes com CONFIGURAR_CALIBRACAO. `tara_interna`, `offset` e `unidade` como parâmetros são rejeitados.

O envelope permanece plano; a única estrutura composta de entrada permitida é o array de strings `ensaio_ids`. Objetos aninhados e outros arrays/tipos não previstos são rejeitados. Números JSON usam ponto decimal; não use strings numéricas.

## Repetições

O ESP retém oito pedidos em RAM, protegendo os pendentes contra remoção. Mesmo ID/sessão/comando/parâmetros reapresenta a resposta original e eventual evento terminal, sem executar novamente. Trocar parâmetros com o mesmo ID gera ID_CONFLITO. Isso cobre metadados, paginação, seleção de ensaio, blocos e exclusões. Ordem das chaves e escapes equivalentes em strings não mudam a identidade; ordem dos IDs no array faz parte do pedido. Parâmetro opcional omitido e parâmetro explicitamente enviado podem ser considerados pedidos diferentes. O coeficiente é comparado numericamente em double, preservando a equivalência de 1000 e 1e3.

Se um ID já consumido saiu do cache, retorna ID_EXPIRADO e não executa novamente. Erros de envelope/parâmetros antes de reservar ID não o consomem; erros de execução são retidos. Use novo ID para novas consultas, novas tentativas após corrigir uma falha operacional e próxima página/bloco. Repetir STATUS devolve o retrato anterior, não atualiza a consulta.

Após reconectar: IDENTIFICAR e STATUS com ID novo. Sessão diferente significa reset: os arquivos persistem, o cache de pedidos não. Não converta automaticamente uma exclusão pendente da sessão antiga em um novo pedido. Confira o catálogo primeiro. Exclusão total não é retomada após reset.

## Listagem

LISTAR_ENSAIOS responde `fase: aceito` e mantém seu ID pendente. A varredura avança por uma entrada de diretório por passagem. O evento terminal LISTA_ENSAIOS contém o mesmo ID, `fase: concluido`, `ensaios`, `apos`, `proximo`, `mais` e `catalogo_revisao`.

A página contém no máximo quatro resumos com ID, formato, condição, nome, tamanho, contagem disponível e indicação de uso. A ordem é lexicográfica por ID. Para a próxima página envie `apos` igual ao `proximo` anterior e `revisao` igual à revisão retornada; `apos` não vazio exige revisão. Comece sem esses dois campos. Revise a paginação quando receber CATALOGO_ALTERADO.

A revisão muda ao criar, fechar ou excluir registros, e é relativa ao boot. Amostras de um ensaio ativo continuam sendo gravadas: suas contagens podem crescer enquanto o catálogo é percorrido. A listagem não é uma fotografia atômica de todas as medições em andamento.

Somente uma operação extensa de catálogo (listagem ou exclusão total) pode estar ativa. STATUS informa `catalogo.ocupado`, operação, solicitação e contadores. Em falha da varredura, o evento FLASH_LISTAGEM termina com fase falhou, código, origem e descrição; não apresenta a página como completa.

## Detalhes e blocos

CONSULTAR_ENSAIO devolve o objeto `ensaio` com os dados documentados em FORMATO_E_RECUPERACAO.md. Pode consultar metadados do ensaio ativo sem alterar a gravação. Arquivo inexistente ou que não pode ser aberto gera ENSAIO_INDISPONIVEL.

LER_AMOSTRAS devolve:

```json
{"protocolo":2,"tipo":"resposta","sessao":"0123456789ABCDEF","ms":12345,"id":10,"comando":"LER_AMOSTRAS","ok":true,"fase":"concluido","ensaio_id":"0123456789ABCDEF-1","bloco_id":"0123456789ABCDEF-1:1:4","inicio":1,"condicao":"concluido","total_disponivel":2,"amostras":[{"indice":1,"valida":true,"tempo_ms":0,"empuxo_kgf":1.5},{"indice":2,"valida":true,"tempo_ms":100,"empuxo_kgf":1.6}],"quantidade":2,"proximo":3,"fim":true,"erros":0}
```

`bloco_id` combina ID do ensaio, início e limite pedido. Use o ID do ensaio e índice da amostra para evitar duplicação no computador. A próxima posição indica o próximo slot, inclusive se algum slot retornou erro. O total disponível conta registros completos pela estrutura do arquivo; cada CRC e identidade são verificados na recuperação. Um registro corrompido recebe `valida: false`, código/origem/descrição, sem tempo ou empuxo inventados. Se houve erro, `erros > 0`; o envelope ok confirma que a consulta foi processada.

Consultar `inicio = total_disponivel + 1` retorna bloco vazio e fim true. Posição posterior gera BLOCO_FORA_LIMITE. A consulta de blocos não altera arquivos e é recusada para arquivo em gravação/transmissão, incluindo pacote LoRa pendente. Não há sessão de download persistente: cada bloco é autossuficiente e o arquivo fica fechado ao terminar a resposta. Se outra operação o excluir entre blocos, o próximo pedido informará a indisponibilidade.

A amostra espontânea USB/LoRa permanece:

```json
{"protocolo":2,"tipo":"amostra","ensaio_id":"0123456789ABCDEF-1","indice":1,"tempo_ms":0,"empuxo_kgf":1.5}
```

## Exclusões

EXCLUIR_ENSAIOS aceita de um a quatro IDs distintos. A validação de todo o pedido ocorre antes da primeira remoção. Os resultados individuais contêm ensaio_id, ok, codigo, origem e descricao. O comando não é uma transação de tudo ou nada: arquivos não protegidos podem ser excluídos enquanto outros falham ou estão protegidos. Confira `todos_excluidos` e cada item.

EXCLUIR_TODOS exige o texto de confirmação. A resposta inicial é aceito; cada resultado intermediário tem código EXCLUSAO_RESULTADO, fase andamento, ID original e sequência. O evento EXCLUSAO_TOTAL_CONCLUIDA informa contadores e todos_excluidos. Fase concluido informa que a operação terminou de percorrer seus candidatos, não que arquivos protegidos foram removidos. Falhas de varredura terminam em fase falhou.

A exclusão total percorre páginas internas de até quatro IDs. Diretórios são fechados antes das remoções; a próxima página é procurada depois do último ID, evitando depender do comportamento do iterador durante exclusões. Arquivos protegidos na aceitação são preservados durante toda a operação; novas operações de início/envio/exclusão são bloqueadas até ela terminar. Aquisição/envio existentes, FINALIZAR, CANCELAR e consultas continuam sendo atendidos.

O cache reapresenta a aceitação e o evento terminal, não todo o fluxo intermediário de exclusões. A serial não possui ACK por evento. Para reconciliação após perda de conexão, consulte STATUS e liste os registros restantes. Erros de exclusão não causam formatação.

## Fases, erros e limites

Respostas instantâneas: concluido. Operações demoradas: aceito, seguido de evento terminal. Aquisição: AQUISICAO_INICIADA após a primeira amostra; ENSAIO_ENCERRADO ao terminar, com duração, contagem e motivo. A tara aplica o offset somente após dez leituras; o timeout continua sendo 3000 ms. Sensor sem leituras por 2000 ms invalida tara e encerra aquisição como falha. Rádio sem TX_DONE por 5000 ms é desabilitado, sem impedir a gravação.

Erros possuem codigo, origem e descricao. Rejeições usam fase rejeitado; falhas durante operação/fechamento usam falha ou evento terminal falhou. Eventos sem comando atual mantêm o ID da operação quando disponível; erros assíncronos gerais podem ter ID null.

| Grupo novo/alterado | Códigos |
| --- | --- |
| Parâmetros | PARAMETRO_INVALIDO, CAMPO_DESCONHECIDO, ID_CONFLITO |
| Catálogo | CATALOGO_OCUPADO, CATALOGO_ALTERADO, FLASH_DIRETORIO, FLASH_LISTAGEM |
| Identificação | ENSAIO_ID_OBRIGATORIO, ENSAIO_ID_COLISAO, ENSAIO_ID_ESGOTADO, ENSAIO_INDISPONIVEL |
| Leitura/versão | FORMATO_NAO_SUPORTADO, BLOCO_FORA_LIMITE, FLASH_LEITURA, FLASH_AMOSTRA_INVALIDA, FLASH_ENSAIO_NAO_CONCLUIDO |
| Exclusão | EXCLUIDO, ENSAIO_PROTEGIDO, NAO_ENCONTRADO, FLASH_EXCLUSAO |
| Gravação/espaço | FLASH_ESPACO, FLASH_SEM_ESPACO, FLASH_CABECALHO, FLASH_GRAVACAO, FLASH_FECHAMENTO |

Continuam os erros de RX, JSON, sessão, IDs, sensor, calibração e rádio da V0.15. A mensagem de emergência JSON_MEMORIA pode omitir alguns campos quando não houver memória ou espaço de serialização; as respostas normais foram verificadas contra o limite.
