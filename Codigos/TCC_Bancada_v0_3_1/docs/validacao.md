# Validação da entrega v0.3.1

Data: 10 de novembro de 2026.

## Verificações executadas

- **Compilação real do firmware:** Arduino CLI, alvo `esp32:esp32:esp32doit-devkit-v1`, Espressif **3.3.12**, LoRa **0.8.0**, HX711 Arduino Library **0.7.5** e LittleFS/FS do pacote da placa. Resultado: sucesso. Sketch: **348588 bytes**; variáveis globais: **23340 bytes**. Os números podem variar conforme versão e opções de compilação.
- **44 testes automatizados aprovados**, com Python 3.12.14, Matplotlib 3.10.8 e pySerial 3.5. Comando: `python -m unittest discover -s tests -v`. Um dos testes compila e executa o fonte C++ com periféricos simulados, usando g++; em máquinas sem esse compilador, esse teste é explicitamente pulado.
- No firmware simulado: ensaios consecutivos de 30 s com intervalos de 95, 13 e 100 ms, taxa irregular, valores positivos/negativos/zero, telemetria ativa e passagem pelo transbordamento de millis(). Todos atingiram o prazo previsto pelo relógio simulado.
- Interrupções simuladas por HX711 sem dados em aproximadamente 3/7/12 s, parada manual, erro de escrita e memória cheia foram distinguidas do fim por duração.
- Leitura que cruza o prazo não é incluída depois da janela. STOP repetido preserva o instante final já congelado.
- No controlador Python: relógio independente da amostra e da fonte do gráfico; tempo final persistido e reaberto; falta de relógio não inventa duração; início recuperado por CLOCK; confirmação de início ausente distinguida de parada do usuário; LIMIT precoce identificado como inconsistente; nova sessão zera o estado e ignora relógio anterior.
- Receptor BRAVO fornecido preservado sem alterações de código ou pinagem.

Os testes anteriores foram novamente executados e abrangem aquisição/controlador USB, confirmação de início/fim, reinício, erro de disco, ausência de memória, parser BRAVO, CRC, limites ADC, saturação, duplicatas, sessões diferentes, comparação USB/LoRa, CSV/JSON, registros incompletos, serial fragmentada por portas virtuais, simulador anterior e transferência de arquivo. A transferência Python foi exercitada com perda de blocos, repetição, retomada após fechamento, ordem invertida, ausência dos marcadores S/E, CRC32 incorreto, duplicatas conflitantes e registro original sem encerramento.

## Limites da verificação

Não houve acesso ao ESP32, à célula ou à estação física do usuário nesta entrega. O teste C++ usa periféricos e passagem de tempo simulados; não verifica SPI, RF, alimentação, interrupção física DIO0, gravação real na flash ou precisão física do relógio. A compilação para ESP32 verifica compatibilidade de código e bibliotecas, não funcionamento elétrico. A causa da interrupção relatada pelo usuário ainda requer ensaio.json/eventos.jsonl de um teste que terminou cedo.

O controlador da interface foi exercitado sem janela. A apresentação visual das novas janelas e a execução no Windows do usuário ainda precisam ser conferidas. O alcance do rádio, a taxa efetiva, o tempo de transferência e as perdas reais ainda não foram medidos. Não há calibração ou validação de precisão em kg/N.

## Primeiro teste de aceitação

1. Conectar apenas o USB da bancada e selecionar Sem estação.
2. Preparar LittleFS, se necessário, e manter Gravar também no ESP32 marcado.
3. Fazer três ensaios consecutivos de 30 s, aplicar/retirar uma carga pequena e verificar a curva bruta USB.
4. Confirmar DURAÇÃO ATINGIDA e reabrir os arquivos do PC. Se houver interrupção, preservar ensaio.json e eventos.jsonl para diagnóstico. Ver `tempo_v0_3_1.md`.
5. Conectar a estação BRAVO ao notebook receptor e abrir RECEPTOR_LORA.bat.
6. Na bancada, selecionar o registro e usar Enviar por LoRa.
7. Conferir RECEBIDO E VERIFICADO, CSV e curva no notebook receptor.
8. Comparar o número de amostras e, se desejado, os bytes do LOG recebido pelo rádio com o mesmo LOG baixado pelo USB.

Se faltar algum bloco, reenviar o mesmo registro e confirmar que as faltas diminuem. O envio local concluído não basta para aprovar a recepção. Um registro original incompleto continua sendo identificado como incompleto mesmo quando sua transferência chega integralmente.
