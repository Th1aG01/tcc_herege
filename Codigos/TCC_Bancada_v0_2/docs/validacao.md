# Validação da entrega v0.2.0

Data: 9 de novembro de 2026.

## Verificações executadas

1. **Compilação real do firmware** com Arduino CLI, alvo `esp32:esp32:esp32doit-devkit-v1`, pacote Espressif **3.3.12**, LoRa **0.8.0**, HX711 Arduino Library **0.7.5** e LittleFS/FS fornecidos pelo pacote da placa. Resultado: sucesso. Sketch: 345848 bytes; variáveis globais: 23252 bytes. Esses números podem mudar com versões e opções de compilação.
2. **24 testes automatizados aprovados**, em Python 3.12.14, Matplotlib 3.10.8 e pySerial 3.5. Execução: `python -m unittest discover -s tests -v`.
3. Importação e verificação de sintaxe dos módulos Python.
4. Comparação byte a byte da cópia do receptor com o arquivo BRAVO fornecido: preservado, sem adaptação dos pinos da estação.

Os testes abrangem: ciclo de início/confirmação/leitura/encerramento; USB sem falsa recepção LoRa; reinício da bancada; ausência de memória; encerramento de arquivos após erro de disco; parser BRAVO; CRC; pacotes truncados; limites de 24 bits e saturação; telemetria reduzida sem falsas lacunas; duplicatas, sessões diferentes e divergências; CSV/JSON; detecção de linha removida; arquivo sem encerramento; registro interno com CRC e contagem; leitura serial fragmentada em portas virtuais POSIX e recuperação após linha excessiva. Os oito testes do simulador anterior também foram mantidos.

## O que ainda precisa ser verificado na bancada

A compilação e os testes em computador não substituem o ensaio físico. Não houve acesso ao ESP32, à célula de 5 kg ou à estação do usuário nesta entrega. O controlador da interface foi testado sem janela; a execução visual desta nova versão no Windows ainda precisa ser conferida pelo usuário. A interface anterior em simulação já havia sido executada por ele.

Primeiro teste de aceitação: manter a bancada e a estação em duas portas USB distintas; iniciar 60 s de aquisição; aplicar e retirar pequena carga; verificar variação bruta no USB e nos pacotes LoRa; conferir aumento do contador de coincidências; encerrar; reabrir `ensaio.json`; baixar e abrir o registro interno. Depois, conferir se a quantidade de amostras do arquivo interno corresponde ao total informado pelo ESP32 no encerramento.

A taxa real de aquisição, o alcance do rádio, a estabilidade da alimentação e a gravação durante falta de energia ainda não foram medidos. Não há validação metrológica, calibração, garantia de precisão em massa/força ou ensaio de acionamento de motor nesta versão.

## Limites deliberados da versão

- Célula de 5 kg registrada como capacidade informada pelo usuário; isso não é um limite de força que o software consegue verificar sem calibração.
- Valores reais somente em contagens. Referência sem carga não é calibração.
- Até 2 pacotes LoRa/s, com seleção da leitura mais recente; não há retransmissão nem ACK de recepção pelo rádio.
- O fim automático por retorno próximo de zero ainda não está implementado. Encerramento por botão, duração ou falha detectada.
- Flash interna LittleFS, sem flash externa nesta etapa. Pode haver perda de dados recentes em corte de energia e arquivos incompletos.
- Não controla ignição nem qualquer atuador.
