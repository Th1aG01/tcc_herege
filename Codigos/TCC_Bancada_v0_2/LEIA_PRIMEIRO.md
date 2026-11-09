# Bancada de empuxo — integração v0.2.0

Esta versão integra a célula de **5 kg, ainda sem calibração**, o HX711, o ESP32, o LoRa da bancada e a estação receptora BRAVO. É uma versão para testes de aquisição na protoboard. Não comanda ignição nem possui saída para iniciar um motor.

## O que você verá

- Leituras reais do HX711 em **contagens**, sem valores inventados em kg ou N.
- Gráfico selecionável: **USB da bancada** ou **LoRa da estação**. Ao selecionar LoRa, o gráfico só mostra pacotes recebidos pela estação; não usa o USB como substituto.
- Contador das leituras que chegaram pelos dois caminhos com a mesma sequência, tempo e valor.
- Início e encerramento pelo computador, com confirmação da bancada.
- CSVs separados para USB e LoRa, metadados JSON e registro de eventos no computador.
- Registro bruto na memória interna do ESP32, com consulta e download pelo aplicativo.
- Simulador anterior disponível em `SIMULACAO.bat`, separado da aquisição real.

## 1. Gravar o firmware na bancada

1. Extraia **todo** o ZIP para uma pasta do Windows. Não execute dentro do ZIP.
2. No Arduino IDE, instale/confira **LoRa por Sandeep Mistry**, versão 0.8.0, e **HX711 Arduino Library por Bogdan Necula / bogde**. Não confunda com outras bibliotecas de nome semelhante.
3. Abra `firmware/Bancada_TCC_v0_2/Bancada_TCC_v0_2.ino`.
4. Selecione **DOIT ESP32 DEVKIT V1** e a porta COM **da bancada**. Use o pacote de placas `esp32` da Espressif já instalado.
5. Envie o programa. O novo firmware usa **115200 baud**, inclusive na bancada.
6. A estação receptora pode continuar com o programa BRAVO que você já testou. A cópia em `firmware/LoRaReceiver_BRAVO/` está **sem alterações**, incluindo os pinos do receptor. Não grave o firmware da bancada nela.

Pinos usados **somente na bancada**:

| Ligação | GPIO do ESP32 |
|---|---:|
| HX711 DT / DAT | 33 |
| HX711 CLK / SCK | 32 |
| LoRa SCK | 18 |
| LoRa MISO | 19 |
| LoRa MOSI | 23 |
| LoRa NSS / CS | 21 |
| LoRa RESET | 14 |
| LoRa DIO0 | 26 |
| CS reservado para futura flash externa | 13, mantido alto |

Mantenha a alimentação e o GND comum da montagem já testada. Conecte a antena adequada antes de transmitir. Para estes testes use cargas pequenas, sem impactos, bem abaixo da capacidade de 5 kg informada para a célula.

## 2. Preparar o aplicativo no Windows

1. Execute `PREPARAR.bat` uma vez. Ele cria um ambiente `.venv` nesta pasta e instala Matplotlib e pySerial.
2. Feche o **Monitor Serial e o Serial Plotter do Arduino**, assim como outros programas usando as portas COM.
3. Execute `INICIAR.bat`.
4. Conecte o USB da bancada e o USB da estação ao computador.
5. Selecione a COM de cada equipamento e clique em **Conectar**. As duas portas precisam ser diferentes. Para identificá-las, desconecte uma por vez e use **Atualizar portas**, antes de conectar no aplicativo.
6. Aguarde a bancada responder. Selecione **USB da bancada** para ver a leitura de prévia enquanto o ensaio ainda não começou.

O aplicativo solicita início/encerramento **pela porta USB da bancada**. A estação BRAVO recebe rádio e repassa texto por seu USB. Ela não precisa transmitir comandos de volta.

## 3. Preparar a memória interna, se necessário

A opção **Gravar também no ESP32** vem marcada. A memória precisa estar pronta para iniciar com essa opção.

- Se o indicador de memória mostrar `OK`, não é necessário preparar nada.
- Se mostrar `UNAVAILABLE`, clique em **Preparar memória do ESP32…**. A confirmação explica que uma partição que não puder ser montada será formatada e poderá perder arquivos antigos.
- Na inicialização normal, o firmware nunca formata automaticamente uma memória que falhou.
- Se quiser primeiro verificar somente o fluxo USB/LoRa, pode desmarcar a gravação no ESP32. Os arquivos do computador continuam sendo salvos. O JSON registra que a cópia interna não foi solicitada.
- Não altere a partição do ESP32 nem use “Erase All Flash” para resolver erros sem antes exportar dados que deseja preservar. O firmware usa a partição de arquivos chamada `spiffs`, montada como LittleFS. A configuração padrão da placa costuma incluí-la; se a preparação falhar, envie o erro e a configuração de partições.

## 4. Fazer o primeiro teste

1. Deixe a célula montada na posição de teste e sem carga adicional por alguns segundos.
2. Clique em **Definir referência sem carga**, se desejar. O aplicativo calcula a média de até 20 leituras recentes e registra esse valor. Isso só define um zero relativo; **não calibra a célula**. A leitura bruta original é preservada.
3. Mantenha o limite inicial de **60 segundos** e dê um nome ao teste.
4. Clique em **Iniciar aquisição**. Aguarde aparecer `ADQUIRINDO`.
5. Selecione **LoRa da estação** no gráfico. Aplique e retire uma carga pequena, de maneira suave.
6. Observe a mudança das contagens, os pacotes LoRa e o contador **Coincidem**. Ao aumentar, ele confirma que aquela leitura bruta recebida pelo rádio corresponde à mesma leitura do USB.
7. Clique em **Encerrar aquisição**, ou aguarde o limite de duração. O aplicativo espera a confirmação do ESP32 e ainda recebe os últimos pacotes por um segundo.
8. O diretório do ensaio aparece na parte inferior. **Abrir teste salvo…** abre `ensaio.json` e seus dois CSVs correspondentes.

Para conferir somente o hardware sem o rádio, selecione `Sem estação` antes de conectar. Isso testa USB/HX711, mas não confirma recepção LoRa.

### Como interpretar o resultado

- Uma leitura sem carga pode ser negativa, positiva ou oscilar. O valor bruto não é uma massa.
- `VARIAÇÃO À REFERÊNCIA` é a diferença em contagens. Não representa kg nem N.
- **LoRa não precisa exibir todas as amostras do USB.** Esta versão transmite a leitura mais recente em até dois pacotes por segundo. A aquisição USB e a memória interna registram cada amostra que o firmware efetivamente lê.
- `Lacunas USB` usa a sequência de amostras. `Lacunas de pacotes LoRa` usa outro contador, exclusivo dos envios; saltos de amostra decorrentes da telemetria reduzida não são tratados como perda de pacote.
- `TX_DONE` indica conclusão local no transmissor. A recepção é demonstrada pelos pacotes da **porta da estação** e pelas comparações na interface.
- `SAT` significa valor no limite digital do ADC. Ele é registrado, sinalizado e não desenhado como leitura válida na curva.
- O software não infere a força, não garante taxa fixa de 80 Hz e não detecta com certeza toda falha possível de fiação. Tempo e sequência permitem avaliar a taxa efetivamente adquirida.
- O ensaio não termina automaticamente quando o valor se aproxima de zero nesta versão. Termina pelo botão, limite configurado ou falha detectada. O encerramento do registro não é um comando para parar um motor.

## 5. Conferir o registro do ESP32

Após encerrar, clique em **Arquivos no ESP32…**, selecione o identificador do ensaio e clique em **Baixar selecionado**. Escolha a pasta de destino. O arquivo `.log` conserva os valores brutos, a sequência, os tempos e o CRC das amostras.

O aplicativo confere o tamanho transferido, a sequência e o CRC antes de concluir o download. Use **Abrir teste salvo…** para abrir o `.log`. O arquivo original continua no ESP32. Exclusão só ocorre pelo botão específico, com confirmação.

Se faltar energia ou o ESP32 reiniciar, os registros anteriores não são apagados automaticamente. O arquivo em andamento pode ficar incompleto, e dados ainda não persistidos podem se perder. Arquivos sem marcador final são identificados como incompletos; arquivos com linha parcial/corrompida são preservados para análise e não apresentados como registro íntegro.

## Arquivos e próximos passos

Cada teste cria uma pasta nova dentro de `%USERPROFILE%\TCC_Bancada\ensaios`, ou da pasta escolhida na interface:

| Arquivo | Conteúdo |
|---|---|
| `usb.csv` | Leituras recebidas diretamente da bancada |
| `lora.csv` | Leituras recebidas pela estação, com RSSI |
| `ensaio.json` | Nome, notas, célula informada, referência, portas, limite, estado e estatísticas |
| `eventos.jsonl` | Comandos, respostas e mensagens recebidas |

Mantenha esses arquivos juntos. Não use os dados reais desta versão para afirmar precisão em kg, empuxo em N ou impulso. A calibração com referência conhecida e os perfis de diferentes células são a próxima etapa, após validar este caminho de aquisição. A flash externa e a lógica opcional de detecção do fim de carga também permanecem para etapas posteriores.

Mais detalhes: `docs/protocolo_v0_2.md` e `docs/validacao.md`.
