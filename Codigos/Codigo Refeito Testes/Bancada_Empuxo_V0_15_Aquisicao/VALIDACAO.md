# Validação V0.15

## Executado nesta entrega

**78/78 cenários nativos aprovados**: 55 regressões de protocolo, calibração e tara, mais 23 cenários de aquisição. O código executado nos testes é o próprio `.ino` entregue, incluído em um programa C++ com hardware simulado.

Compilação nativa com g++ C++17, `-Wall -Wextra -Werror -pedantic`, AddressSanitizer e UndefinedBehaviorSanitizer. ArduinoJson 7.4.2 real; ESP/UART, HX711, flash e LoRa simulados. LeakSanitizer desativado devido à incompatibilidade com ptrace no ambiente.

Foi feita também verificação de sintaxe com os cabeçalhos oficiais LoRa 0.8.0 e HX711 de bogde, mantendo as demais interfaces Arduino simuladas. A API pública de LoRa continua sendo usada; não há chamada a isTransmitting privado. As assinaturas de File.flush/close/seek e LittleFS.totalBytes/usedBytes foram conferidas no core Arduino-ESP32 3.3.3. **Não foi realizada compilação completa com o toolchain ESP32, upload ou teste em placa.**

## Novos cenários

| Cenário | Verificação |
| --- | --- |
| aq_inicio_real | Aceitação em PREPARANDO, LED apagado, evento único após primeira amostra, tempo zero e deduplicação |
| aq_ids_distintos | IDs próprios diferentes entre ensaios; requisição separada; repetição não trunca |
| aq_mesma_medicao | Mesmo valor negativo calibrado na USB, arquivo e LoRa; JSON idêntico; nova calibração não recalcula arquivo |
| aq_frequencias_independentes | 150 amostras enquanto rádio permanece ocupado com apenas um pacote |
| aq_nao_reenvia_ultima | Falta de nova leitura não causa retransmissão repetida da mesma amostra ao vivo |
| aq_finalizacao | Contagem, duração, motivo, IDs de início/parada, fechamento, LED e idempotência |
| aq_espera_timeout | Sensor para antes da primeira amostra; falha sem anunciar início real |
| aq_cancelar_preparacao | Parada antes de amostras não vira ensaio normalmente concluído |
| aq_flash_sem_espaco | Rejeição de início sem substituir registro anterior |
| aq_cabecalho_falha | Falha na preparação não aceita aquisição |
| aq_amostra_parcial | Amostra parcial não é publicada/contada; fechamento inválido; envio recusado |
| aq_rodape_falha | Rodapé parcial gera erro correlacionado e evento terminal de falha |
| aq_flush_falha | Simulação de perda de byte no flush detectada na releitura |
| aq_sensor_registro_falho | Falha de sensor registrada como falha, preservada na recuperação; envio normal recusado |
| aq_reconectar | 500 leituras adicionais com saída USB bloqueada; reconexão consulta mesmo ensaio sem reiniciar |
| aq_recuperacao_boot | Rotina de inspeção recupera registro normal e distingue registro sem rodapé |
| aq_arquivo_antigo | Arquivo sem formato 2 não é reinterpretado como normal |
| aq_espaco_esgota | Reserva estimada de encerramento; falta de espaço encerra como falho |
| aq_corrupcao_amostra | Índice inconsistente interrompe envio, antes de transmitir a amostra |
| aq_limites_json | ID longo, valores extremos, tamanho do pacote/status e esgotamento de contador |
| aq_radio_falha_continua | Sem TX_DONE, rádio é desabilitado e aquisição continua até FINALIZAR |
| aq_falha_reabertura | Falha ao reabrir impede confirmar fechamento normal |
| aq_sensor_antigo_inicio | Leitura antiga bloqueia novo início sem apagar registro anterior |

As 55 regressões incluem entradas válidas, fragmentadas, incompletas, desconhecidas e malformadas; tamanho/timeout de recepção; deduplicação; sessão e IDs; congestionamento de saída; configuração positiva/negativa e rejeição de coeficientes inválidos; tara assíncrona, ausência/recuperação de sensor e correção sem duplo offset/fator. Os testes de regressão que não tratam calibração usam uma configuração válida como fixture; os testes cal_* exercitam os comandos reais desde a configuração padrão bloqueada.

A recuperação de boot foi testada chamando a mesma rotina sobre arquivos simulados, após limpar o resumo em RAM. Não foi simulada a atomicidade real do LittleFS nem efetuado corte físico de alimentação. A falha de flush é uma injeção no modelo, não um erro retornado pela API real, que é void.

## Ainda depende do hardware

- Compilar na Arduino IDE com o core e as bibliotecas instalados e fazer upload.
- Confirmar coeficiente e tara com sua célula, ruído, sinal, escala e estabilidade.
- Medir frequência efetiva e intervalos das amostras; HX711, latência da flash e agendamento real não são reproduzidos pelos mocks.
- Verificar LEDs, rádio/DIO0, TX_DONE e recepção remota do novo JSON.
- Verificar fechamento/retensão do arquivo na flash real, inclusive após reset e falta de energia.
- Fechar/reabrir a serial com alimentação mantida e observar se a abertura da COM aciona reset por DTR/RTS.
- Confirmar a política de montagem/partição já usada: LittleFS interno, begin(true), um arquivo substituído a cada novo ensaio.

As leituras usam is_ready antes de get_value/read, como na V0.14. Comportamento elétrico anômalo durante uma leitura e capacidade de manter cada conversão do HX711 precisam ser verificados na placa. A ausência de um host não é confundida com a perda da alimentação do ESP.

## Reprodução opcional

Não é necessário executar estes testes para abrir o sketch. Dependências: g++, Python 3 e ArduinoJson 7.4.2. Na raiz extraída:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Ivalidacao_nativa/mocks -I/caminho/ArduinoJson-7.4.2/src \
  validacao_nativa/test_firmware.cpp -o validacao_nativa/test_firmware
python3 validacao_nativa/run_tests.py
```

`resultados.json` registra cada resultado, e os `.jsonl` são as saídas simuladas. As sessões neles não são a sessão de sua placa. Nenhuma biblioteca de terceiros ou binário de teste foi empacotado.

Referências primárias conferidas:

- https://github.com/sandeepmistry/arduino-LoRa/blob/0.8.0/src/LoRa.h
- https://github.com/bogde/HX711/blob/master/src/HX711.h
- https://github.com/bogde/HX711/blob/master/src/HX711.cpp
- https://github.com/espressif/arduino-esp32/blob/3.3.3/libraries/FS/src/FS.h
- https://github.com/espressif/arduino-esp32/blob/3.3.3/libraries/LittleFS/src/LittleFS.h
