# Validação física — V0.18

**Status: pendente. Este roteiro não afirma que os testes abaixo foram executados.**
Manter o circuito de ignição fisicamente desabilitado em toda a etapa. Não conectar motor/ignitor.

## 1. Registrar ambiente e preservar dados

Anotar placa, versão do core Arduino-ESP32, bibliotecas, partição, tamanho de Flash interna,
alimentação, posição de RATE do HX711 e porta COM. Usar os valores da V0.17 que funcionava.
Guardar o ZIP original e exportar os ensaios internos importantes pela V0.17 antes do upload.
Não mudar partições nem habilitar apagamento total na IDE. Salvar a saída de compilação.

O JEDEC **EF 40 15 já foi validado** no Checkpoint 04. Não é necessário repetir para
autorizar desenvolvimento. `ferramentas/DiagnosticoFlash` está disponível para diagnóstico
posterior: apenas leitura 9F/status, CS13 e SPI18/19/23, 115200 baud. A primeira verificação
da V0.18 deve continuar somente leitura; não partir diretamente para ensaio gravado.

Conferir visualmente fiação, GND comum, 3V3, WP/HOLD altos e CS distintos. Quando houver
instrumentação, verificar tensão e CS no boot/reset. Fotografar a marcação do CI se acessível.
Essa informação complementa, sem invalidar, a identificação JEDEC já recebida.

## 2. Compilar e subir a configuração somente leitura

Abrir o sketch principal com `FLASH_PERMITIR_ESCRITA 0`. Compilar; se houver erro,
guardar o log completo antes de tentar trocar core/biblioteca ou pinagem.
FQBN de referência: `esp32:esp32:esp32`. O core não está homologado neste pacote.

Depois de upload e reset, usar monitor serial 115200, fim de linha LF ou CRLF.
Enviar:

```json
{"protocolo":2,"tipo":"comando","id":0,"comando":"IDENTIFICAR"}
```

Copiar `sessao` da resposta para os próximos comandos. Os exemplos usam `SESSAO_ATUAL`
como marcador: substituí-lo pelo valor real. Em cada reset a sessão muda.

```json
{"protocolo":2,"tipo":"comando","id":1,"sessao":"SESSAO_ATUAL","comando":"STATUS"}
{"protocolo":2,"tipo":"comando","id":2,"sessao":"SESSAO_ATUAL","comando":"ESPACO"}
```

Verificar firmware 0.18, backend `LittleFS_externo`, JEDEC 15679509, `escrita:false`.
Flash vazia/desconhecida: montagem falha explicitamente, sem formatação. Flash preparada:
monta e permite listar/ler; não modifica o conteúdo ao iniciar. Falha JEDEC exige diagnóstico
de ligação/alimentação, sem adaptar o driver para aceitar IDs desconhecidos.

## 3. Preparação — somente se necessária e autorizada

**Não execute esta etapa sem confirmar que o chip inteiro pode ser inicializado.**
A autorização para desenvolver o firmware não autorizou apagar memórias na bancada.
Este pacote não executou nenhuma operação física.

Se a Flash já contiver LittleFS e `/ensaios`, pular preparação. Se houver conteúdo desconhecido,
preservar e planejar backup/recuperação separadamente. Não escolher “format on fail”.

Para um chip autorizado e integralmente vazio:

1. Abrir e compilar `ferramentas/PrepararFlashVazia/PrepararFlashVazia.ino` como sketch separado.
2. Subir com as mesmas precauções de partição/erase interno. O boot somente identifica.
3. Depois da autorização específica, enviar **exatamente**, com LF:

```text
AUTORIZO_INICIALIZAR_FLASH_VAZIA_EF4015
```

4. A ferramenta varre os 2 MiB e só prossegue se todos forem FF. Aguarde com alimentação
   estável. `PREPARADO` confirma retorno de formato/montagem/criação do diretório;
   ainda é necessário o teste de persistência. Qualquer erro exige registrar a saída.
5. Conteúdo diferente de FF → recusa, sem comando de força. Se a energia cair durante
   preparação e deixar metadados parciais, não tentar contornar essa proteção.
6. Retornar ao firmware principal. A ferramenta não faz ensaios e não deve permanecer
   instalada como firmware operacional.

## 4. Autorizar gravação e registrar dois ensaios curtos

Com backend preparado e verificado, alterar `FLASH_PERMITIR_ESCRITA` para `1` e recompilar
o principal. Isso habilita gravação em resposta a comandos; não formata no boot.
Após cada upload/reset, IDENTIFICAR novamente. STATUS deve informar `escrita:true`.

Configurar o **coeficiente real já determinado para a célula utilizada**, seguido de TARA
sem carga. Abaixo, substituir `VALOR_NUMERICO_VALIDADO` por um número JSON, sem aspas:

```text
{"protocolo":2,"tipo":"comando","id":1,"sessao":"SESSAO_ATUAL","comando":"CONFIGURAR_CALIBRACAO","coeficiente":VALOR_NUMERICO_VALIDADO}
```

```json
{"protocolo":2,"tipo":"comando","id":2,"sessao":"SESSAO_ATUAL","comando":"TARA"}
```

Aguardar evento de conclusão da tara. Iniciar, aplicar carga manual conhecida e finalizar
após cerca de 5–10 s (o comando abaixo não agenda duração):

```json
{"protocolo":2,"tipo":"comando","id":3,"sessao":"SESSAO_ATUAL","comando":"INICIAR","nome":"V018_A","observacoes":"Validacao sem ignicao"}
{"protocolo":2,"tipo":"comando","id":4,"sessao":"SESSAO_ATUAL","comando":"FINALIZAR"}
```

Enviar FINALIZAR somente após o intervalo desejado, não as duas linhas juntas.
Guardar `ensaio_id`, eventos, número de amostras, duração, coeficiente e tara.
Repetir com IDs 5/6 e nome V018_B; não repetir um ID para uma operação diferente.
LED verde deve acompanhar aquisição; armazenamento deve funcionar sem receptor.

## 5. Recuperar e comparar após desligar/religar

Antes de desligar, aguardar fechamento confirmado. Consultar e recuperar ambos:

```json
{"protocolo":2,"tipo":"comando","id":7,"sessao":"SESSAO_ATUAL","comando":"LISTAR_ENSAIOS"}
{"protocolo":2,"tipo":"comando","id":8,"sessao":"SESSAO_ATUAL","comando":"CONSULTAR_ENSAIO","ensaio_id":"ID_REAL_DO_ENSAIO"}
{"protocolo":2,"tipo":"comando","id":9,"sessao":"SESSAO_ATUAL","comando":"LER_AMOSTRAS","ensaio_id":"ID_REAL_DO_ENSAIO","inicio":1,"limite":4}
```

Avançar `inicio` de quatro em quatro, usando novos IDs, até recuperar todos os registros.
Guardar respostas. Desligar/religar a alimentação, identificar nova sessão e repetir.
Comparar IDs, metadados, amostras, índices, timestamps e valores; nenhuma recalibração
deve recalcular arquivos antigos. CRCs são verificados na leitura de blocos; registrar
qualquer diagnóstico de corrupção. O firmware não continua aquisição após reset.
ESPACO deve manter capacidade nominal de 2097152 B e ocupação compatível.

Para comparação **binária byte a byte**, usar o arquivo reconstruído por um receptor BT/1
compatível quando disponível. A leitura JSON dos blocos permite comparar valores, mas
não constitui exportação bruta de todos os bytes de cabeçalho/rodapé.

## 6. Rádio, autonomia e limites

- Com receptor BT/1, `ENVIAR` deve incluir `ensaio_id`. Consultar andamento, cancelar
  e repetir; confirmar que o arquivo permanece e que a reconstrução/CRC final coincide.
- Sem receptor, o esperado é término limitado sem confirmação remota; isso não invalida
  armazenamento nem prova recepção. Diferenciar `ENVIO_SEM_CONFIRMACAO` de `LORA_TX_TIMEOUT`.
- Testar INICIAR_LORA com HX711, observando LED amarelo e compartilhamento do SPI.
  Se disponível, usar analisador lógico: nenhum instante com ambos CS ativos;
  medir duração de programas/erases, pausas entre amostras e transição TX/RX.
- Desconectar USB mantendo alimentação independente estável, terminar/reconectar conforme
  os controles disponíveis e verificar ensaio. Remover USB que também alimenta o ESP32 é
  corte de energia, não teste de independência da comunicação. Evitar reset por DTR/RTS.
- Medir intervalos dos timestamps e taxa efetiva no RATE configurado do HX711. Os testes
  de software não homologam 10/80 Hz; um erase pode causar perda de conversões.
- Memória cheia: preferir inicialmente o cenário nativo. Na placa, preencher apenas com
  ensaios de teste autorizados, sem apagar existentes; confirmar recusa explícita e arquivos
  anteriores legíveis. Não há comando oculto de preenchimento físico no firmware.
- Corte durante aquisição: só após backup, usando ensaio de teste autorizado. Verificar
  arquivo incompleto/recuperável, dados anteriores intactos e ausência de retomada automática.
  Registrar quanto foi perdido desde o último sync; não extrapolar para todo tipo de falha elétrica.
- Exclusões: testar apenas IDs criados para essa finalidade, mediante escolha explícita.
  Não usar EXCLUIR_TODOS em memória com ensaios importantes; as proteções e confirmações
  do protocolo continuam exigidas.

## Registro de aceite

| Verificação | Resultado inicial | Evidência a anexar |
|---|---|---|
| JEDEC EF4015 anterior | VALIDADO FISICAMENTE pelo usuário, Checkpoint 04 | Monitor serial já apresentado |
| Compilação V0.18/placa | PENDENTE | Core, bibliotecas, FQBN, log |
| Boot somente leitura | PENDENTE | STATUS/ESPACO e observação de SPI |
| Dois ensaios + reboot | PENDENTE | IDs, respostas e comparação |
| CRC/reconstrução BT/1 | PENDENTE | Arquivo, hash e confirmação remota |
| Autonomia sem USB | PENDENTE | Alimentação e dados persistidos |
| Taxa/pausas HX711 | PENDENTE | Timestamps/analisador lógico |
| Cheia/falhas/corte | PENDENTE | Logs e preservação dos ensaios anteriores |

Só avançar para integração Python após compilar e validar o armazenamento básico nesta placa.
