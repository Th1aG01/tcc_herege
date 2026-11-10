# Validação da entrega v0.3.0

Data: 9 de novembro de 2026.

## Verificações executadas

- **Compilação real do firmware:** Arduino CLI, alvo `esp32:esp32:esp32doit-devkit-v1`, Espressif **3.3.12**, LoRa **0.8.0**, HX711 Arduino Library **0.7.5** e LittleFS/FS do pacote da placa. Resultado: sucesso. Sketch: **348200 bytes**; variáveis globais: **23324 bytes**. Os números podem variar conforme versão e opções de compilação.
- **33 testes automatizados do aplicativo aprovados**, com Python 3.12.14, Matplotlib 3.10.8 e pySerial 3.5. Comando: `python -m unittest discover -s tests -v`.
- Verificação de sintaxe/importação dos módulos Python.
- Exercício do código C++ do firmware em ambiente de teste com periféricos substituídos: envio completo, cancelamento e timeout sem falsa conclusão.
- Os quadros produzidos pelo código C++ foram entregues ao receptor Python. Dois blocos foram omitidos, o receptor foi reaberto e uma nova passagem em ordem inversa completou o arquivo. **Os 1537 bytes e as 30 amostras do registro sintético recuperado corresponderam ao original.**
- Receptor BRAVO fornecido preservado sem alterações de código ou pinagem.

Os testes Python abrangem aquisição/controlador USB, confirmação de início/fim, reinício, erro de disco, ausência de memória, parser BRAVO, CRC, limites ADC, saturação, duplicatas, sessões diferentes, comparação USB/LoRa, CSV/JSON, registros incompletos, serial fragmentada por portas virtuais, simulador anterior e transferência de arquivo. A transferência foi exercitada com perda de blocos, repetição, retomada após fechamento, ordem invertida, ausência dos marcadores S/E, CRC32 incorreto, duplicatas conflitantes e registro original sem encerramento.

## Limites da verificação

Não houve acesso ao ESP32, à célula ou à estação física do usuário nesta entrega. O teste C++ usa periféricos simulados; não verifica SPI, RF, alimentação, interrupção física DIO0 ou gravação real na flash. A compilação para ESP32 verifica compatibilidade de código e bibliotecas, não funcionamento elétrico.

O controlador da interface foi exercitado sem janela. A apresentação visual das novas janelas e a execução no Windows do usuário ainda precisam ser conferidas. O alcance do rádio, a taxa efetiva, o tempo de transferência e as perdas reais ainda não foram medidos. Não há calibração ou validação de precisão em kg/N.

## Primeiro teste de aceitação

1. Conectar apenas o USB da bancada e selecionar Sem estação.
2. Preparar LittleFS, se necessário, e manter Gravar também no ESP32 marcado.
3. Iniciar 10 s, aplicar/retirar uma carga pequena e verificar a curva bruta USB.
4. Confirmar encerramento e reabrir os arquivos do PC.
5. Conectar a estação BRAVO ao notebook receptor e abrir RECEPTOR_LORA.bat.
6. Na bancada, selecionar o registro e usar Enviar por LoRa.
7. Conferir RECEBIDO E VERIFICADO, CSV e curva no notebook receptor.
8. Comparar o número de amostras e, se desejado, os bytes do LOG recebido pelo rádio com o mesmo LOG baixado pelo USB.

Se faltar algum bloco, reenviar o mesmo registro e confirmar que as faltas diminuem. O envio local concluído não basta para aprovar a recepção. Um registro original incompleto continua sendo identificado como incompleto mesmo quando sua transferência chega integralmente.
