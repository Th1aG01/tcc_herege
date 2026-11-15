# Validação V0.13 — USB JSON

## Executado

**35 de 35 cenários aprovados.** Os testes compilam e executam o próprio `.ino`, com ArduinoJson 7.4.2 real e simuladores de Arduino/ESP, UART, HX711, SPI, LittleFS e LoRa. O método privado `isTransmitting()` continua privado no simulador; o firmware não o chama.

Compilação nativa com g++ C++17, `-Wall -Wextra -Werror -pedantic`, AddressSanitizer e UndefinedBehaviorSanitizer. Nenhum erro reportado na execução final. LeakSanitizer desativado por incompatibilidade do ambiente com ptrace.

Também foi feita verificação de sintaxe usando **LoRa.h oficial 0.8.0**, com as demais interfaces de hardware simuladas. Isso verifica o acesso à API pública da biblioteca, mas não substitui compilar para a arquitetura ESP32.

| Cenários | Resultado exercitado |
| --- | --- |
| Identidade e configuração | Identificação, firmware/protocolo, estados, 115200 baud e preservação dos pinos/parâmetros |
| Fragmentação, CRLF e várias linhas | Fragmentos de um byte, ausência de execução antes de LF e recuperação entre linhas |
| Antigos, desconhecidos, esquema inválido | Letras antigas nunca executam operações; erros estruturados |
| JSON malformado, lixo, chaves duplicadas | Rejeição de sintaxe permissiva, objetos concatenados, duplicatas inclusive com escape Unicode |
| Timeout, tamanho e controles | Limites exatos 512/513 bytes, timeout, NUL e recuperação após descarte |
| Sessão errada, IDs repetidos e conflitos | Sem efeitos colaterais na repetição; rejeição de sessão anterior e de ID com outro comando |
| Expiração e operação pendente | IDs retirados do histórico não reexecutam; registros pendentes permanecem protegidos |
| Alternância, erros e consultas repetidas | Alternância ocorre uma vez; erro retido não vira nova tentativa; consulta repetida reproduz o snapshot |
| Envio, cancelamento e falhas | Aceitação separada do evento terminal; IDs originais preservados; falhas de flash, sensor e TX_DONE |
| Inicialização | USB responde durante a tara e na ausência do sensor |
| Congestionamento | Saída com capacidade zero/limitada não bloqueia a aquisição; amostras descartadas são contadas; controle aguarda espaço |
| Amostras | Fórmula, índice e correlação com o ID de início |
| Ordem de campos | Solicitação equivalente com campos reordenados não repete a ação |
| Fuzz determinístico | 1.000 linhas geradas, até 599 bytes, sem execução indevida; depois aceita comando válido |

Na execução final foram analisadas **1.237 linhas JSON**. O teste de fila congestionada demonstrou que 200 amostras continuaram sendo gravadas no simulador enquanto a capacidade de escrita USB era zero. Isso verifica a lógica, não uma taxa física garantida.

## Pendente na placa

- Compilação completa na Arduino IDE com o core ESP32 e as bibliotecas instaladas no seu computador.
- Upload, porta COM, fragmentação real da USB/UART e comportamento do driver.
- Precisão/ruído da célula, calibração em kgf, taxa de aquisição e latências reais de flash/serial.
- Identificação por eFuse e sessão gerada com `esp_random()` no ESP32 real.
- DIO0/GPIO 26, interrupção TX_DONE, LEDs e recepção LoRa.
- Reset, quedas de alimentação e comportamento prolongado.

A montagem/formatação do LittleFS e operações de flash ainda são síncronas, como na base. O protocolo limita o trabalho de RX/TX e a tara inicial, mas não transforma todo o hardware em um sistema de tempo real nem assegura ausência de atrasos em drivers. Um erro extremo de RAM ou de transporte pode impedir a entrega de mensagens; respostas retidas e contadores tornam recuperáveis os casos normais de perda/congestionamento.

## Reprodução opcional

Fontes e resultados estão em `validacao_nativa/`. São opcionais para quem só usa a Arduino IDE. Dependências: g++, Python 3 e o diretório `src` do ArduinoJson 7.4.2; não acompanham o ZIP.

Na raiz extraída, substitua o caminho da dependência:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Ivalidacao_nativa/mocks -I/caminho/ArduinoJson-7.4.2/src \
  validacao_nativa/test_firmware.cpp -o validacao_nativa/test_firmware
python3 validacao_nativa/run_tests.py
```

As linhas JSON produzidas estão nos arquivos `.jsonl`; `resultados.json` registra cada resultado. As identidades/sessões presentes nesses registros são simuladas e não devem ser copiadas para operar a sua placa.

Referências das APIs: [ArduinoJson deserializeJson](https://arduinojson.org/v7/api/json/deserializejson/), [UART ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/serial.html), [getEfuseMac](https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/Esp.h), [esp_random](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/random.html), [LoRa 0.8.0](https://github.com/sandeepmistry/arduino-LoRa/blob/0.8.0/src/LoRa.h).
