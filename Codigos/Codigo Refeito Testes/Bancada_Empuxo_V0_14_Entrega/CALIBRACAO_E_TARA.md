# Calibração e tara — comandos e testes V0.14

## Formato

Protocolo 2, 115200 baud, um objeto JSON por linha, terminado por LF/CRLF. Mantenha os IDs crescentes para solicitações novas. Para repetir uma solicitação cuja resposta se perdeu, use a mesma sessão, ID, comando e coeficiente. Consulte o protocolo completo no arquivo correspondente.

No Monitor Serial, selecione **Nova linha**. Primeiro descubra a sessão:

```json
{"protocolo":2,"tipo":"comando","id":0,"comando":"IDENTIFICAR"}
```

Nos exemplos abaixo, **substitua `0123456789ABCDEF` pela sessão recebida**. Os IDs pressupõem uma sessão nova; em outra situação, continue a partir do maior ID já consumido. Execute uma linha de cada vez.

### 1. Consultar o valor inicial

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":1,"comando":"CONSULTAR_CALIBRACAO"}
```

Depois do boot, o resultado esperado é coeficiente 1.0, `configurada: false`, `valida: false`, `origem: "padrao_inicial"` e `pronta_para_uso: false`. A tara automática pode estar em andamento, concluída ou ter falhado. A calibração permanece bloqueada mesmo se a tara inicial concluir.

### 2. Configurar o coeficiente em kgf

Espere a tara inicial terminar ou falhar antes desta operação:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":2,"comando":"CONFIGURAR_CALIBRACAO","coeficiente":1000.0}
```

**1000.0 é apenas exemplo de protocolo**, não um coeficiente determinado para sua célula. Para medições reais, substitua pelo coeficiente calibrado em contagens/kgf do perfil selecionado no computador.

A resposta terá `fase: "concluido"`, `calibracao.configurada: true`, `calibracao.valida: true`, `origem: "comando_usb"` e `tara.pronta: false`. O início ainda fica bloqueado até a tara.

Também são válidos valores como `-1000`, `1234.5` e `-1.2345e3`. Use **ponto decimal**, como exige JSON, e não coloque o número entre aspas. O firmware usa números representáveis em `double`; rejeita zero, valores que resultem em zero por underflow e valores não finitos. Não há limite arbitrário de sinal ou magnitude para um número finito e não zero.

O coeficiente é lido diretamente do token numérico em `double`, evitando uma passagem intermediária por `float` que algumas versões do ArduinoJson podem usar ao interpretar decimais curtos. O cálculo não usa o fator `float` interno do HX711. A serialização dos números segue o formato/precisão do ArduinoJson.

### 3. Executar tara sem carga

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":3,"comando":"TARA"}
```

A primeira resposta tem `fase: "aceito"`, `tara.em_andamento: true` e `tara.pronta: false`. Ela não confirma sucesso da tara.

Em seguida, o ESP coleta dez leituras, no máximo uma por passagem do loop. Ao completar, calcula a média bruta e aplica o offset apenas internamente. O evento de conclusão tem `id: 3`, `codigo: "TARA_CONCLUIDA"`, `fase: "concluido"`, `tara.pronta: true` e os indicadores de calibração.

Se o HX711 não fornecer leituras suficientes dentro de **3000 ms**, aparece `HX711_TARA_TIMEOUT` e depois o evento `TARA_FALHOU`, `fase: "falhou"`, com ID 3. A tara permanece inválida. Uma ausência momentânea de dado pronto não é tratada como desconexão imediata: a operação aguarda as leituras dentro desse limite.

O offset anterior não é substituído por uma média parcial. Mesmo que continue armazenado internamente, ele não pode liberar um ensaio após uma tara malsucedida, porque `tara.pronta` permanece false.

### 4. Conferir prontidão

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":4,"comando":"STATUS"}
```

| Campo | Significado |
| --- | --- |
| `calibracao.coeficiente` | Valor numérico atual |
| `calibracao.configurada` | Houve configuração explícita aceita nesta inicialização |
| `calibracao.valida` | Configuração explícita, finita e diferente de zero |
| `calibracao.origem` | `padrao_inicial` ou `comando_usb` |
| `calibracao.unidade` | Sempre `kgf`; não é uma opção recebida |
| `calibracao.pronta_para_uso` | Calibração válida, tara pronta, nenhuma tara em andamento e sensor com leituras recentes |
| `calibracao.validacao_fisica` | A precisão física é responsabilidade do operador; não é comprovada pela validação numérica |
| `tara.pronta` | Offset obtido com sucesso e não invalidado desde então |
| `tara.em_andamento` | Coleta de tara ativa |
| `tara.leituras` / `necessarias` | Progresso da operação atual ou última tara; confirme sempre o campo `pronta` |
| `tara.timeout_ms` | Limite total da operação: 3000 |

Esses grupos aparecem em `STATUS`, `CONSULTAR_CALIBRACAO`, nas confirmações de ações e nos eventos terminais de operações. `sensor.tara_pronta` e `sensor.tara_em_andamento` foram mantidos no status para compatibilidade. Se o sensor deixar de responder, a prontidão para uso fica falsa; a detecção de timeout também invalida a tara.

### 5. Iniciar e finalizar

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":5,"comando":"INICIAR"}
```

Aguarde e confira as amostras. Depois:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":6,"comando":"FINALIZAR"}
```

Durante a aquisição, tentativas novas de `TARA` e `CONFIGURAR_CALIBRACAO` são recusadas sem alterar coeficiente, offset ou estado da tara. Consultar não interrompe o ensaio.

## Repetições e erros de parâmetro

- Mesmo ID e mesmo coeficiente numérico: resposta anterior, sem reconfigurar. `1000` e `1e3` correspondem ao mesmo valor. Se uma tara válida foi concluída depois da configuração, repetir o ID antigo não a invalida; a resposta repetida continua sendo o retrato original. Consulte com novo ID para estado atual.
- Mesmo ID e outro coeficiente: `ID_CONFLITO`, sem alteração.
- `TARA` repetida com o mesmo ID: não reinicia contador ou prazo; reapresenta aceitação e eventual conclusão.
- Nova configuração aceita, mesmo com o mesmo número e outro ID: invalida a tara e exige uma nova.
- Nova tentativa após erro de execução exige novo ID; repetir o ID de uma falha recupera o erro anterior.
- Erro de envelope/parâmetro antes de reservar o ID não altera a calibração anterior. Por exemplo, zero ou string numérica são rejeitados, preservando um coeficiente e uma tara já válidos.

`coeficiente` só é permitido em `CONFIGURAR_CALIBRACAO`, onde é obrigatório. Não há `offset`, `unidade`, identificação de perfil ou campos aninhados. Um perfil deve ser traduzido no computador apenas para o coeficiente em kgf.

| Entrada ou condição | Resultado |
| --- | --- |
| `coeficiente: 0` ou `-0.0` | `COEFICIENTE_INVALIDO` |
| `coeficiente: "1000"` ou `"NaN"` | `COEFICIENTE_INVALIDO` |
| Número JSON que transborde para infinito, como `1e309` | `COEFICIENTE_INVALIDO` |
| Literais `NaN`, `Infinity`, booleanos, null ou JSON inválido | `JSON_INVALIDO` |
| Campo `offset` ou `unidade` | `CAMPO_DESCONHECIDO` |
| Início sem calibração explícita válida | `CALIBRACAO_NAO_CONFIGURADA` |
| Início sem tara pronta ou durante tara | `TARA_NAO_PRONTA` |
| Alteração de coeficiente ou tara durante aquisição | `ENSAIO_ATIVO` |
| Alteração de coeficiente ou nova tara enquanto outra tara avança | `TARA_EM_ANDAMENTO` |
| Tara por comando sem dez leituras em três segundos | `HX711_TARA_TIMEOUT` + evento `TARA_FALHOU` |
| Ausência de leituras na tara automática do boot | `HX711_TARA_INICIAL_TIMEOUT`; USB continua disponível |

## Por que não há correção duplicada

Na biblioteca HX711 de bogde:

- `read()` devolve a leitura bruta;
- `get_value()` devolve a leitura menos o offset interno;
- `get_units()` ainda divide esse resultado pelo fator de escala interno.

A V0.14 usa **`read()` para determinar a tara**, `set_offset()` para aplicá-la e **`get_value() / coeficiente` para medir**. Não usa `get_units()` nem `set_scale()` e não subtrai offset fora da biblioteca. O ganho do ADC, mantido no padrão da biblioteca, é diferente do coeficiente de calibração.

Nos testes simulados: tara bruta 1200, leitura bruta 2500 e coeficiente 1000 resultaram em 1.3 kgf; com coeficiente -1000, em -1.3 kgf. Uma segunda tara com leitura bruta 1800 substituiu o offset por 1800, sem descontar a primeira tara antes de calcular a segunda.

Esses números verificam a lógica matemática, não uma calibração física da sua célula. Referência: [implementação oficial HX711](https://github.com/bogde/HX711/blob/master/src/HX711.cpp).
