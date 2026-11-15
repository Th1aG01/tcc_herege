# Validação V0.14

**55/55 cenários nativos aprovados:** 35 regressões do protocolo/firmware anterior e 20 cenários novos de calibração e tara. Os testes de regressão preparam explicitamente um estado de calibração válido como fixture para testar o restante do protocolo; os novos cenários começam sem essa configuração e exercitam os comandos JSON reais.

Foi compilado e executado o `.ino` desta entrega com g++ C++17, `-Wall -Wextra -Werror -pedantic`, AddressSanitizer e UndefinedBehaviorSanitizer. ArduinoJson 7.4.2 é real; ESP/UART, HX711, flash e LoRa são simulados. LeakSanitizer permanece desativado por incompatibilidade com ptrace neste ambiente.

## Novos casos exercitados

- Valor padrão 1.0 bloqueia início; configuração explícita 1.0 é aceita, mas exige tara.
- Coeficientes positivos, negativos, decimais, notação científica e extremos finitos.
- Rejeição de zero, strings numéricas, NaN/Infinity, null, booleanos, overflow e underflow para zero, sem perder uma configuração válida anterior.
- Rejeição de offset e seleção de unidade.
- Comparação de parâmetro na repetição: mesmo ID/valor não reconfigura; valor diferente conflita.
- Bloqueio de coeficiente e tara durante aquisição, com preservação do valor e do offset.
- Aceitação assíncrona de tara, progresso, evento terminal e preservação do ID.
- Repetição de tara em andamento e depois da conclusão não reinicia nem reaplica a operação.
- Média de leituras brutas e segundo cálculo de tara sem descontar o offset anterior.
- Ausência de sensor, leituras parciais, timeout, preservação do offset anterior como inválido e recuperação com novo comando.
- USB disponível durante tara e com sensor ausente no boot.
- Configuração bloqueada durante tara; tara e envio de gravação avançam simultaneamente com IDs independentes.
- Coeficiente recebido em fragmentos, sem execução antes de LF.
- Tamanho das respostas com coeficiente de grande magnitude e IDs no limite de 32 bits.

Os testes de matemática verificam o resultado interno em double e a amostra JSON com tolerância numérica apropriada, pois números decimais não são em geral exatos em ponto flutuante. O coeficiente recebido é extraído diretamente do token em double para evitar uma conversão intermediária para float na interpretação do JSON.

A verificação de sintaxe também usa os cabeçalhos oficiais de **LoRa 0.8.0 e HX711 de bogde**, com as interfaces Arduino restantes simuladas. Foi consultado o código oficial do HX711 para conferir a separação entre `read`, `get_value`, `get_units`, `set_offset` e `set_scale`.

## Ainda depende da sua placa

Compilação completa na Arduino IDE, upload, porta COM, precisão/ruído/deriva da célula, taxa real do HX711, estabilidade da tara, tempos de flash/UART, interrupção DIO0 e transmissão/recepção LoRa. A tara usa `is_ready()` antes de cada leitura; comportamento elétrico anômalo durante uma leitura precisa ser avaliado no hardware. Não foi determinada uma calibração física para sua célula.

O coeficiente e os flags são voláteis. Depois de reset/upload, a sessão muda e o ESP exige configuração e tara novamente. A validade matemática/operacional reportada pelo firmware não comprova a exatidão física do perfil informado.

## Reprodução opcional

Fontes e resultados em `validacao_nativa/`; não são necessários para carregar o firmware. Dependências externas: g++, Python 3 e ArduinoJson 7.4.2. Na raiz extraída:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Ivalidacao_nativa/mocks -I/caminho/ArduinoJson-7.4.2/src \
  validacao_nativa/test_firmware.cpp -o validacao_nativa/test_firmware
python3 validacao_nativa/run_tests.py
```

`resultados.json` lista todos os casos; os arquivos `.jsonl` guardam saídas simuladas. Identidade/sessão desses testes não são os valores da sua placa.
