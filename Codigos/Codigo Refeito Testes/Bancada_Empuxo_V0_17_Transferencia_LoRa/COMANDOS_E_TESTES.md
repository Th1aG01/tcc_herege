# Comandos e testes pela serial — V0.17

Monitor Serial: **115200 baud**, final de linha **Nova linha**. Envie cada objeto em uma única linha. Exemplos pressupõem uma sessão nova: substitua **0123456789ABCDEF** pela sessão real e use IDs crescentes maiores que o `maior_id` recebido. Substitua também **ID_DO_ENSAIO**, **ID_A** e **ID_B** pelos identificadores devolvidos pelo ESP.

## 1. Descobrir, configurar e fazer tara

```json
{"protocolo":2,"tipo":"comando","id":0,"comando":"IDENTIFICAR"}
```

Aguarde a tara inicial concluir ou falhar. Configure o coeficiente do seu perfil:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":1,"comando":"CONFIGURAR_CALIBRACAO","coeficiente":1000}
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":2,"comando":"TARA"}
```

**1000 é apenas exemplo, não uma calibração determinada para sua célula.** Use seu coeficiente em contagens/kgf, positivo ou negativo, finito e não zero. Faça tara sem carga e aguarde `TARA_CONCLUIDA` do pedido 2. Nova configuração invalida a tara; offset não é recebido pela interface.

## 2. Gravar dois ensaios

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":3,"comando":"INICIAR","nome":"Ensaio A","data":"2026-11-15","observacoes":"Teste com carga conhecida","celula_id":"CELULA_20KG","perfil_id":"PERFIL_COMPRESSAO"}
```

Primeiro vem a resposta `aceito`, estado PREPARANDO, com `ensaio.id`. Guarde esse identificador. O evento `AQUISICAO_INICIADA` confirma a primeira amostra gravada e acende o LED verde. Observe algumas amostras e finalize:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":4,"comando":"FINALIZAR"}
```

Confira `ENSAIO_ENCERRADO` com o ID original 3, `fase: concluido`, duração, contagem e fechamento confirmado. Crie outro ensaio:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":5,"comando":"INICIAR","nome":"Ensaio B","data":"2026-11-15","observacoes":"Segunda medicao","celula_id":"CELULA_20KG","perfil_id":"PERFIL_COMPRESSAO"}
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":6,"comando":"FINALIZAR"}
```

Envie FINALIZAR **depois de observar amostras**, não imediatamente após INICIAR. O ID do ensaio B deve ser diferente do A. Não é necessário reconfigurar/tarar entre esses dois ensaios se a configuração e a tara continuarem válidas e a montagem não tiver mudado.

## 3. Listar em páginas

Para facilitar a observação, peça um item por página:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":7,"comando":"LISTAR_ENSAIOS","limite":1}
```

Recebe `aceito` e depois o evento `LISTA_ENSAIOS`, com `ensaios`, `mais`, `proximo` e `catalogo_revisao`. Copie `proximo` para `apos` e a revisão recebida para `revisao` na próxima solicitação:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":8,"comando":"LISTAR_ENSAIOS","limite":1,"apos":"ID_DO_ENSAIO","revisao":5}
```

**5 é exemplo; use a revisão retornada.** Repita com novos IDs até `mais: false`. A ordem é lexicográfica dos IDs, não a ordem de data ou o nome. Sem parâmetros, limite 4 e cursor inicial vazio. Não envie a próxima página antes do evento terminal da atual.

Se ocorrer CATALOGO_ALTERADO, reinicie a listagem sem `apos`/`revisao`, com novo ID de pedido. Início, fechamento e exclusão mudam a revisão. A contagem de um ensaio ainda ativo pode crescer entre consultas.

## 4. Conferir metadados e recuperar amostras

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":9,"comando":"CONSULTAR_ENSAIO","ensaio_id":"ID_DO_ENSAIO"}
```

Confira nome, data, observações, célula/perfil, coeficiente, `tara_interna`, unidade kgf, condição, duração e contagens. `tara_interna` é uma referência registrada em contagens do ADC; consultá-la não muda a tara atual, e enviá-la como parâmetro é rejeitado.

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":10,"comando":"LER_AMOSTRAS","ensaio_id":"ID_DO_ENSAIO","inicio":1,"limite":4}
```

A resposta contém `bloco_id`, ID do ensaio, início, quantidade, total disponível, próxima posição, `fim`, contagem de erros e as amostras. Cada amostra válida traz índice, tempo e empuxo em kgf. Para continuar, use `proximo` como `inicio` em um pedido de ID novo. Quando `fim: true`, terminou.

Se a resposta se perder, repita **exatamente o mesmo pedido e ID** para recuperar a resposta retida, sem tratar o bloco repetido como dados novos. Um ID já usado com outro início/limite/ensaio gera ID_CONFLITO.

Blocos também podem ser recuperados de registros incompletos com cabeçalho íntegro. Amostras corrompidas aparecem com `valida: false`, código, origem e descrição; não recebem empuxo inventado. O restante do bloco continua identificável. A leitura do arquivo em gravação ou transmissão é recusada até ele ser liberado.

## 5. Verificar preservação após reiniciar

1. Finalize os dois ensaios e anote seus IDs e contagens.
2. Reinicie o ESP. A sessão de comunicação mudará; os arquivos não devem mudar.
3. Envie IDENTIFICAR sem sessão, ID 0. Use a nova sessão e IDs de pedidos novos para LISTAR_ENSAIOS.
4. Consulte os dois IDs antigos com CONSULTAR_ENSAIO. Nome, perfil, coeficiente, referência de tara, amostras e condição concluída devem permanecer.
5. Recupere blocos e compare índice, tempo e kgf com os dados anteriores.

Não é necessário configurar calibração ou fazer tara para listar/ler/excluir gravações existentes. Para uma **nova aquisição**, configure novamente o coeficiente e faça tara, pois esses estados operacionais continuam voláteis. STATUS não escolhe automaticamente um ensaio antigo como último: o catálogo é a fonte de consulta após reset.

Para conferir interrupção, crie um terceiro ensaio de teste, aguarde amostras e reinicie **sem FINALIZAR**. Após redescobrir a sessão, ele deve aparecer como incompleto se o cabeçalho tiver sido preservado. Os dois ensaios anteriores devem continuar concluídos. LER_AMOSTRAS recupera os registros completos com CRC válido. Um corte físico de alimentação pode perder os últimos dados em cache; essa durabilidade precisa ser verificada na bancada.

A duração final de um ensaio sem rodapé válido é desconhecida (`duracao_ms: null`). `duracao_observada_ms` informa o tempo da última amostra completa e íntegra, quando recuperável, e não finge ser o instante exato da interrupção.

## 6. Consultar espaço e transmitir um ensaio escolhido

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":11,"comando":"ESPACO"}
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":12,"comando":"ENVIAR","ensaio_id":"ID_DO_ENSAIO"}
```

ESPACO retorna bytes totais/usados/livres, reserva de 8192 bytes e espaço estimado após a reserva. A margem ajuda a concluir operações, mas não promete que toda capacidade livre seja utilizável como amostras; o filesystem precisa de blocos e metadados.

ENVIAR exige ensaio_id, registro de formato 3, fechamento normal e rádio disponível. O LED amarelo indica TX local. Sem um receptor BT/1, o ESP repetirá INICIO e terminará com ENVIO_SEM_CONFIRMACAO. CANCELAR aborta o TX local e libera o arquivo. Consulte TESTES_LORA.md para iniciar, consultar e cancelar com os novos campos e distinguir envio local de confirmação remota.

Para compatibilidade com o roteiro anterior, ENVIAR sem ensaio_id usa apenas o último ensaio conhecido em RAM nesta inicialização. Depois de reset esse atalho não escolhe arquivos: use sempre o identificador explícito no novo fluxo.

## 7. Excluir selecionados

**Este comando remove os arquivos indicados.** Use IDs de ensaios descartáveis para o teste:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":13,"comando":"EXCLUIR_ENSAIOS","ensaio_ids":["ID_A","ID_B"]}
```

A resposta informa o resultado de cada ID: EXCLUIDO, ENSAIO_PROTEGIDO, NAO_ENCONTRADO ou FLASH_EXCLUSAO. Confira `todos_excluidos`; `ok: true` no envelope significa que o comando produziu seus resultados, não que todos os arquivos necessariamente foram apagados.

Teste protegendo um arquivo com aquisição ou envio LoRa em andamento. Ele deve permanecer. Outros IDs não protegidos do mesmo pedido podem ser excluídos. IDs repetidos no mesmo array, caminhos como `../arquivo`, arrays vazios ou mais de quatro IDs são rejeitados antes de qualquer exclusão.

## 8. Excluir todos explicitamente

**Use apenas quando quiser remover todos os registros não protegidos do catálogo, inclusive o legado:**

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":14,"comando":"EXCLUIR_TODOS","confirmacao":"EXCLUIR_TODOS"}
```

Sem a confirmação textual exata, o comando é rejeitado. Após aceitação, cada arquivo gera EXCLUSAO_RESULTADO com ID do pedido, sequência e resultado individual. O evento EXCLUSAO_TOTAL_CONCLUIDA informa excluídos, protegidos, falhas e `todos_excluidos`.

Arquivos protegidos quando o comando foi aceito continuam protegidos durante essa operação, mesmo que o ensaio/envio termine nesse intervalo. Não há tentativa posterior silenciosa de excluí-los. Para removê-los depois, envie um novo comando explícito.

Durante a exclusão total, novos inícios, novos envios gravados e outras exclusões são recusados com CATALOGO_OCUPADO. Uma aquisição já ativa continua; FINALIZAR e CANCELAR permanecem disponíveis. A operação avança por etapas e espera espaço na fila de resposta quando necessário.

Se desconectar, redescubra a sessão e consulte STATUS: `catalogo` informa operação e contadores. Repetir o pedido retido reapresenta aceitação e evento terminal, sem executar de novo; os resultados intermediários individuais não são todos guardados no cache. Liste novamente para conferir os arquivos restantes. Após reset, a exclusão total não é retomada automaticamente.

## Entradas inválidas e resultados esperados

| Entrada/condição | Resultado esperado |
| --- | --- |
| Linha fragmentada ainda sem LF | Nenhuma ação; aguarda conclusão até timeout |
| JSON inválido, duplicação de chaves, objeto aninhado | JSON_INVALIDO |
| Comando inexistente com envelope válido | COMANDO_DESCONHECIDO |
| Metadado acima do limite, data impossível, ID inseguro, limite 0/5 | PARAMETRO_INVALIDO, sem executar a ação |
| offset/tara/unidade como parâmetros de INICIAR | CAMPO_DESCONHECIDO |
| Repetir ID com metadados ou IDs de exclusão diferentes | ID_CONFLITO |
| Falta de espaço para outro ensaio | FLASH_SEM_ESPACO; anteriores preservados |
| Montagem falha | FLASH_INICIALIZACAO; nenhuma formatação automática |
| Registro legado ou formato desconhecido solicitado em LER_AMOSTRAS | FORMATO_NAO_SUPORTADO; arquivo preservado |
| Falha ao percorrer diretório | Evento FLASH_LISTAGEM, fase falhou; não declara página/exclusão completa |

O arquivo VALIDACAO.md identifica quais desses cenários foram simulados. Upload, LEDs, flash física, retomada da COM, ruído do sensor e recepção LoRa continuam dependendo de sua bancada.
