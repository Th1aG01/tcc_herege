# Validação V0.16

## Executado

**118/118 cenários nativos aprovados:** 78 regressões de protocolo/calibração/aquisição, adaptadas às mudanças deliberadas de formato e armazenamento, e 40 cenários novos de múltiplos ensaios, metadados, recuperação e exclusão.

Os testes incluem o próprio .ino e os dois cabeçalhos desta entrega. Compilação nativa g++ C++17 com `-Wall -Wextra -Werror -pedantic`, AddressSanitizer e UndefinedBehaviorSanitizer. ArduinoJson 7.4.2 é real; ESP, UART, HX711, flash e LoRa são simulados. LeakSanitizer está desativado por incompatibilidade com ptrace neste ambiente.

Também foi realizada verificação de sintaxe usando os cabeçalhos oficiais LoRa 0.8.0 e HX711 de bogde, com o restante do Arduino simulado. Não há chamada ao método privado LoRa.isTransmitting. As interfaces de arquivos/diretórios foram conferidas no código oficial Arduino-ESP32: getNextFileName está presente nos cabeçalhos 2.0.17 e 3.3.3, e a implementação VFS 3.3.3 foi consultada para distinguir término normal da varredura de erro informado por errno.

**Não houve compilação completa com o toolchain ESP32, upload ou teste físico.** Não se conclui destes testes que toda conversão do HX711 será capturada na taxa nominal no hardware.

## Preservação entre reinicializações simuladas

O teste `ar_reboot_gravar` cria dois ensaios concluídos e um terceiro em andamento, com metadados, duas amostras completas e 17 bytes de uma amostra parcial. O estado dos arquivos é exportado para um disco de teste e o processo termina sem FINALIZAR o terceiro ensaio.

`ar_reboot_ler` executa em **outro processo**, com todas as variáveis do firmware reinicializadas, carregando apenas os bytes dos arquivos antes de setup. Verifica:

- Os três arquivos permanecem e LittleFS inicia sem formatação.
- Os dois primeiros continuam concluídos, cada um com três amostras.
- O terceiro é incompleto, com cabeçalho/metadados/tara preservados e duas amostras recuperáveis sem erro.
- Não há retomada automática da aquisição e a calibração operacional volta a não configurada.
- Depois de configurar/tarar, um novo ensaio cria um quarto arquivo, sem modificar um byte dos três anteriores.

O gerador de sessão simulado é determinístico; essa reinicialização também exercita a colisão dos IDs candidatos, obrigando o firmware a escolher um caminho livre. Isso verifica a lógica de preservação mesmo quando candidatos se repetem.

Esse teste é mais forte que apenas limpar um resumo em RAM, mas continua sendo uma simulação: não reproduz caches, atomicidade, desgaste ou perda de energia do chip real.

## Cenários novos

| Grupo | Verificações |
| --- | --- |
| Vários ensaios | IDs distintos, arquivos anteriores byte a byte preservados, catálogo completo e colisões sem sobrescrita |
| Metadados | Nome/data/observações/célula/perfil, UTF-8 e escapes, coeficiente e tara efetivos, preservação depois de outra calibração/tara |
| Validação de entrada | Datas inválidas, limites, NUL, tipos errados, offset/unidade/tara externos, IDs inseguros, arrays vazios/repetidos/excessivos |
| Repetições | Metadados equivalentes com chaves permutadas, conflito de parâmetros, repetição de blocos e exclusões sem novos efeitos |
| Paginação | Nove ensaios percorridos sem lacunas/duplicação, revisão alterada, listagem durante aquisição |
| Blocos | Sequência de nove amostras, identificador de bloco, próxima posição, bloco vazio ao final e posição fora do limite |
| Recuperação | Amostra parcial, cabeçalho corrompido, CRC de amostra/rodapé, tempo da última amostra íntegra e registro incompleto |
| Exclusão selecionada | Sucesso, inexistente, falha de remoção e resultado individual |
| Exclusão total | Onze ensaios, várias páginas internas, resultados por arquivo, confirmação explícita e preservação de arquivo alheio ao catálogo |
| Proteções | Aquisição, envio gravado, último pacote após CANCELAR, último pacote ao vivo após FINALIZAR e proteção mantida durante exclusão total |
| Falhas | Espaço insuficiente, escrita de novo cabeçalho sem afetar anteriores, montagem sem formatação, diretório/varredura/abertura indisponíveis |
| Legado/versões | /ensaio.bin formato 2 preservado, identificado e recusado para leitura nova; versão futura não interpretada |
| Limites de saída | Congestionamento USB sem descarte deliberado de resultados de catálogo, textos/escapes máximos e mensagens dentro do buffer |
| Reboot | Dois processos separados, arquivo interrompido recuperável e novo ensaio preservando todos os anteriores |

O registro exato de cada caso está em `validacao_nativa/resultados.json`; os arquivos `.jsonl` contêm as mensagens produzidas nas simulações. O teste dos maiores textos escapados também usa um cabeçalho válido controlado para verificar o limite de saída independentemente do limite de 512 bytes da entrada.

As regressões continuam cobrindo recepção fragmentada/malformada, não execução por caracteres, deduplicação, calibração de ambos os sinais, tara assíncrona sem dupla correção, falhas de sensor, aquisição independente de transmissão, gravação com USB bloqueada, início real e fechamento.

## Limitações e testes na placa

Ainda precisa ser verificado na bancada:

1. Compilação na Arduino IDE e upload com suas versões instaladas de core e bibliotecas.
2. Montagem da partição LittleFS existente sem formatação e sem alterar seu esquema de partições.
3. Criação de vários ensaios, reset real e comparação das amostras recuperadas.
4. Corte de energia durante gravação, preservação de registros anteriores e quantidade efetivamente durável do registro interrompido.
5. Precisão da calibração/tara, latência de flash, frequência de aquisição, uso de memória e comportamento prolongado.
6. LEDs, DIO0/TX_DONE, recepção LoRa e proteção do arquivo até terminar o último pacote.
7. Reconexão da COM, verificando se o monitor aciona reset por DTR/RTS.

Não há comandos ocultos para injetar falhas no ESP. Erros de escrita, flush, leitura, diretório e exclusão foram simulados nos mocks. File.flush/close têm retorno void: verificações estruturais/CRC ajudam a detectar falhas observáveis, mas não constituem garantia absoluta de durabilidade.

O modelo de espaço conta os bytes dos arquivos; não reproduz a alocação e fragmentação real do LittleFS. A reserva de 8192 bytes é conservadora e as verificações de escrita continuam necessárias. A varredura é cooperativa, mas cada chamada da biblioteca filesystem é síncrona e pode ter latência própria.

## Reprodução opcional

Na raiz extraída, com g++, Python 3 e ArduinoJson 7.4.2 disponíveis:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Ivalidacao_nativa/mocks -I/caminho/ArduinoJson-7.4.2/src \
  validacao_nativa/test_firmware.cpp -o validacao_nativa/test_firmware
python3 validacao_nativa/run_tests.py
```

O executor cria um diretório temporário para o teste de reinicialização entre processos. Nenhum binário, biblioteca de terceiros ou arquivo de disco temporário é incluído na entrega.

Referências primárias consultadas:

- https://github.com/sandeepmistry/arduino-LoRa/blob/0.8.0/src/LoRa.h
- https://github.com/bogde/HX711/blob/master/src/HX711.h
- https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/FS/src/FS.h
- https://github.com/espressif/arduino-esp32/blob/3.3.3/libraries/FS/src/FS.h
- https://github.com/espressif/arduino-esp32/blob/3.3.3/libraries/FS/src/vfs_api.cpp
- https://github.com/espressif/arduino-esp32/blob/3.3.3/libraries/LittleFS/src/LittleFS.cpp
