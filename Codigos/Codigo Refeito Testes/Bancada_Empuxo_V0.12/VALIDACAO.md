# Validação V0.12

- 20/20 cenários de software aprovados, usando o `.ino` desta entrega e ArduinoJson 7.4.2 real, com periféricos simulados.
- Compilação nativa com g++ C++17, avisos tratados como erros, AddressSanitizer e UndefinedBehaviorSanitizer. LeakSanitizer desativado por incompatibilidade com ptrace neste ambiente.
- O simulador LoRa agora declara `isTransmitting()` como privado e fornece o callback público `onTxDone()`. Antes, o simulador expunha o método privado e por isso não detectou o erro relatado.
- Reproduzido o erro da V0.10 com o **LoRa.h oficial 0.8.0**. O firmware V0.12 passou na verificação de sintaxe com o mesmo cabeçalho real, sem chamadas a métodos privados. Demais interfaces Arduino foram simuladas nessa verificação; não é uma compilação para o processador ESP32.
- Verificados envio ao vivo, arquivo gravado, rádio ocupado, LED após finalizar com pacote em andamento e erros de envio.
- Comparadas as constantes de pinagem, rádio e armazenamento com a versão anterior: preservadas.

## Pendente no hardware

Compilação completa na sua Arduino IDE, upload, interrupção TX_DONE pela ligação DIO0/GPIO 26, LEDs físicos, aquisição e recepção LoRa. A validação local não comprova esses comportamentos físicos.

## Reprodução dos testes nativos

Requer g++, Python 3 e o diretório src do ArduinoJson 7.4.2. Na raiz extraída, substitua o caminho da dependência:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -g -Ivalidacao_nativa/mocks -I/caminho/ArduinoJson-7.4.2/src validacao_nativa/test_firmware.cpp -o validacao_nativa/test_firmware
python3 validacao_nativa/run_tests.py
```

O ZIP não inclui dependências de terceiros nem executável de teste. Os resultados estão em validacao_nativa/resultados.json.

Referências: [API pública LoRa](https://github.com/sandeepmistry/arduino-LoRa/blob/0.8.0/API.md), [cabeçalho oficial 0.8.0](https://github.com/sandeepmistry/arduino-LoRa/blob/0.8.0/src/LoRa.h).
