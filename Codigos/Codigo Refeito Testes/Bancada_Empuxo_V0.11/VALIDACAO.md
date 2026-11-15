# Validação — Bancada V0.10 / Etapa 1

Data: 14/11/2026. **20 de 20 cenários nativos aprovados.** Os testes usam o código `.ino` real da entrega, ArduinoJson 7.4.2 real e simuladores de Serial, HX711, SPI, LoRa e LittleFS. Não houve conexão com a bancada.

## Executado nesta entrega

- Compilação do firmware para o computador com `g++`, C++17, `-Wall -Wextra -Werror -pedantic`.
- Execução com AddressSanitizer e UndefinedBehaviorSanitizer, sem erros reportados. A detecção de vazamentos do LeakSanitizer foi desativada porque o ambiente usa ptrace e não permite essa verificação; os outros dois verificadores permaneceram ativos.
- Leitura de todas as mensagens produzidas nos testes com ArduinoJson, verificando que cada linha é JSON válido e identifica o protocolo.
- Comparação das constantes de pinagem, SPI, rádio, intervalo, arquivo, política de montagem e fator padrão com o anexo original.
- Verificação de que somente os dois GPIOs de LED são escritos pelo firmware; não foi acrescentada saída de ignição.

| Cenário | Comportamento exercitado |
| --- | --- |
| `configuracao` | Pinagem, configurações, tara inicial, status, ajuda e CRLF |
| `enquadramento` | Espera por Enter; palavras/JSON/comandos concatenados não viram comandos por caractere |
| `limites` | Limites de 63/64 caracteres, descarte integral, caracteres inválidos e recuperação da próxima linha |
| `medicao` | Fórmula com tara e coeficiente positivo/negativo; JSON e conteúdo do registro gravado |
| `boot_sem_hx` | Inicialização termina após três segundos sem sensor e aceita consultas seriais |
| `tara_incompleta` | Apenas cinco das dez leituras: recusa a tara e não inicia aquisição |
| `timeout_hx` | Ausência de leituras interrompe ensaio uma vez, fecha arquivo e exige nova tara |
| `sem_lora` | Início com rádio indisponível é recusado antes de substituir arquivo; aquisição sem rádio funciona |
| `sem_flash` | Erro de montagem e recusa do início |
| `atomicidade` | Início repetido não reinicia ensaio; comandos incompatíveis preservam estado; ligar/desligar explícitos |
| `falha_abertura` | Erro ao criar arquivo não deixa aquisição ativa |
| `falha_gravacao` | Escrita parcial encerra aquisição e não publica a amostra como gravada |
| `sem_ensaio` | Arquivo ausente recusa envio |
| `retransmissao` | Rádio ocupado adia a mesma amostra; todas são enviadas em ordem; conclusão espera último pacote |
| `falha_leitura` | Falha de leitura encerra envio com erro |
| `falha_seek` | Falha de reposicionamento encerra envio com erro |
| `falha_lora` | Falha de preparação do pacote suspende envio, preservando aquisição quando ao vivo |
| `leds` | Verde durante aquisição; amarelo acompanha TX, inclusive após finalizar com pacote em andamento |
| `coeficiente_invalido` | Recusa de zero/NaN/infinito; interrupção em resultado não finito |
| `tamanho_amostra` | Mensagem compacta com contadores de 32 bits e valor de grande magnitude |

A maior amostra serial exercitada tinha 105 bytes com LF. Com CRLF, seriam 106 bytes; a 80 amostras/s, isso representa 8.480 bytes/s, antes de mensagens de controle. É uma estimativa de tráfego, não uma medição de frequência real ou ausência de perdas. A serial continua sujeita a atraso se o computador parar de consumir dados ou se forem enviados comandos continuamente.

O JSON LoRa exercitado manteve exatamente os três campos anteriores. O conteúdo do anexo V0.9 foi copiado sem alteração para o histórico. SHA-256 do original:

`3c9c2c08a255282d69765fcaf423fc4db48a785986053965e8d2ceca3a0aca8c`

## Ainda depende da sua Arduino IDE e do hardware

| Verificação | Situação |
| --- | --- |
| Compilar para a placa ESP32 e para as versões instaladas das bibliotecas | Não executada; a compilação local foi nativa, com periféricos simulados |
| Upload e abertura da porta COM no Windows | Não executados |
| Tara real, sinal, ruído, deriva e precisão em kgf | Não medidos; coeficiente original 1.0 não está calibrado |
| Taxa real de aquisição e atraso de gravação/serial | Não medidos |
| LEDs físicos e estado de TX reportado pelo rádio | Não observados na placa |
| Montagem, escrita, leitura e retenção reais do LittleFS | Não exercitadas no ESP32 |
| Interpretação de uma gravação anterior com o mesmo compilador/partições | Não verificada na placa |
| Transmissão a 915 MHz e recepção dos pacotes | Não executadas; ausência de ACK remoto preservada |
| Falhas elétricas, quedas de alimentação e comportamento de longo prazo | Não exercitados |

Uma simulação de periféricos não prova o comportamento elétrico nem a compatibilidade completa do core ESP32. O limite da tara depende de o sensor manter um dado pronto durante sua leitura; interrupções físicas nesse instante ainda precisam ser avaliadas no equipamento. Os timeouts detectam ausência de novas leituras, não substituem uma validação da célula.

## Roteiro de aceitação na bancada

1. Compile o sketch e confira a inicialização a 115200 baud. Registre a saída de `STATUS`.
2. Execute a sequência de `LEIA_ME.md`: ajuda, estado, erro intencional, iniciar, aguardar, finalizar.
3. Verifique os LEDs e se o estado e o contador correspondem ao observado. Nesta etapa, avalie principalmente o fluxo de dados; a precisão depende da calibração.
4. Se testar LoRa, use os comandos de `COMANDOS.md` e confira os pacotes no receptor separadamente.
5. Para avaliar o timeout do sensor, altere conexões somente com a placa desligada, ligue novamente e confira o erro e a resposta a `STATUS`.
6. Registre qualquer erro da IDE ou JSON da serial antes da próxima etapa. Não é necessário abrir a interface Python.

## Reprodução opcional dos testes nativos

Os arquivos em `validacao_nativa/` são destinados a validação de software em Linux, sem placa. Eles não são necessários para carregar o firmware. O pacote inclui os fontes dos testes, os simuladores, um resumo JSON e os registros produzidos. Não inclui compilador, executável ou dependências de terceiros.

Use g++ com suporte a C++17, Python 3 e o diretório `src` da distribuição ArduinoJson 7.4.2. Na pasta raiz extraída, substitua `/caminho/ArduinoJson-7.4.2/src` pelo caminho real:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Ivalidacao_nativa/mocks -I/caminho/ArduinoJson-7.4.2/src \
  validacao_nativa/test_firmware.cpp -o validacao_nativa/test_firmware
python3 validacao_nativa/run_tests.py
```

Referências das APIs consultadas: [HX711](https://github.com/bogde/HX711/blob/master/src/HX711.h), [implementação da tara HX711](https://github.com/bogde/HX711/blob/master/src/HX711.cpp), [LoRa](https://github.com/sandeepmistry/arduino-LoRa/blob/master/src/LoRa.h), [estado de TX LoRa](https://github.com/sandeepmistry/arduino-LoRa/blob/master/src/LoRa.cpp), [ArduinoJson serializeJson](https://arduinojson.org/v7/api/json/serializejson/) e [ArduinoJson 7.4.2](https://arduinojson.org/v7/revisions/).
