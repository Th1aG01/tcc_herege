# Atualização v0.4.0

Leia primeiro docs/JSON_v04.md: payload JSON e botão Apagar TODOS na janela Arquivos no ESP32. Atualize o firmware e o aplicativo.

# Bancada de empuxo — v0.4.0

**Prioridade: testar por USB. Depois, enviar o registro salvo por LoRa.**

O PC da bancada controla a aquisição. O notebook do orientador pode receber o arquivo sem estar conectado ao ESP32 da bancada. A célula de teste tem capacidade informada de **5 kg e ainda não está calibrada**: os valores aparecem em contagens do HX711, sem conversão para kg ou N.

## Atualização v0.4.0 — tempo e interrupções

Atualize **o programa do ESP32 e o aplicativo** usando esta pasta. O campo **TEMPO DO ENSAIO** agora recebe o relógio do ESP32, independentemente da chegada de amostras. O horário da última amostra aparece separado. O fim por duração, por comando e por falha é identificado na tela e no arquivo `ensaio.json`.

Para conferir o problema relatado, faça **três aquisições consecutivas de 30 s**, inicialmente só por USB. Deixe **Gravar também no ESP32** marcada, se a memória estiver disponível. Aplique e retire uma carga pequena; retornar ao zero não encerra a aquisição. Ao atingir o prazo, deve aparecer **DURAÇÃO ATINGIDA**, com o tempo informado pelo ESP32 próximo de 30 s. Pode haver pequena latência do laço de execução; não é uma garantia de precisão metrológica do relógio.

Se terminar antes, anote a mensagem de encerramento e envie **`ensaio.json` e `eventos.jsonl` da mesma pasta do teste**. Esses arquivos também existem na versão anterior e ajudam a identificar a causa da interrupção original. O ajuste não presume que haja defeito no sensor: a causa ainda depende desse registro. Detalhes em `docs/tempo_v0_3_1.md`.

## 1. Primeiro teste: somente USB

1. Extraia todo o ZIP para uma nova pasta do Windows.
2. No Arduino IDE, confira **LoRa por Sandeep Mistry 0.8.0** e **HX711 Arduino Library por Bogdan Necula / bogde 0.7.5**.
3. Abra `firmware/Bancada_TCC_v0_3_1/Bancada_TCC_v0_3_1.ino` e envie para o **ESP32 da bancada**, selecionando DOIT ESP32 DEVKIT V1 e sua COM. Compilação verificada com Espressif 3.3.12.
4. Execute `PREPARAR.bat` uma vez. Ele instala as dependências Python em um ambiente nesta pasta. Depois abra `INICIAR.bat`.
5. Feche o Monitor Serial e o Serial Plotter do Arduino antes de conectar no aplicativo.
6. Selecione a COM da bancada. Deixe a estação opcional como **Sem estação** e clique **Conectar**.
7. O gráfico padrão é **USB da bancada**. A telemetria LoRa ao vivo inicia pausada no novo firmware. Falha ou ausência do rádio não impede a aquisição por USB.
8. Se a memória aparecer como `UNAVAILABLE`, clique **Preparar memória do ESP32…**. A confirmação explica que uma partição que não puder ser montada será formatada e poderá perder arquivos antigos. O firmware não formata automaticamente na inicialização.
9. Deixe **Gravar também no ESP32** marcada para enviar esse registro depois pelo rádio. Se desmarcar, o teste USB e os arquivos do PC funcionam, mas não haverá cópia interna desse ensaio para o envio posterior.
10. Deixe a célula montada e sem carga adicional por alguns segundos. Use **Definir referência sem carga**, se desejar. Isso apenas define um zero relativo; não calibra a célula.
11. Para começar, escolha **10 segundos**. Clique **Iniciar aquisição**, aplique e retire uma carga pequena e suave, bem abaixo da capacidade informada, e confira a variação no gráfico.
12. Aguarde o limite ou clique **Encerrar aquisição**. Reabra `ensaio.json` pelo botão **Abrir teste salvo…** para conferir os dados.

Não é necessário conectar um receptor para essa etapa. Não há comando de ignição ou de atuador.

### Pinos da bancada

| Ligação | GPIO |
|---|---:|
| HX711 DT / DAT | 33 |
| HX711 CLK / SCK | 32 |
| LoRa SCK | 18 |
| LoRa MISO | 19 |
| LoRa MOSI | 23 |
| LoRa CS / NSS | 21 |
| LoRa RESET | 14 |
| LoRa DIO0 | 26 |
| CS da futura flash externa | 13, mantido alto |

Mantenha a alimentação e o GND comum já testados. Para transmitir, conecte a antena adequada. O rádio continua em **915 MHz e sync word 145 (0x91)**, conforme a referência BRAVO.

## 2. Preparar o notebook do orientador

O receptor pode continuar com **o código BRAVO fornecido e já testado**. A cópia `firmware/LoRaReceiver_BRAVO/LoRaReceiver_BRAVO.ino` foi preservada, incluindo os pinos próprios da estação. Não grave o programa da bancada no receptor.

No notebook que receberá:

1. Copie e extraia este mesmo pacote.
2. Se necessário, instale Python. Execute `PREPARAR.bat` uma vez.
3. Conecte o receptor LoRa ao USB do notebook e feche outros programas que usam essa COM.
4. Abra **RECEPTOR_LORA.bat**.
5. Selecione a COM **do receptor** e clique **Conectar receptor**.
6. Deixe essa janela aberta. Ela não precisa da COM da bancada.

A entrada esperada é a saída BRAVO `Received packet '...' with RSSI ...`, em **115200 baud**. Também são aceitas linhas de payload TCCF3 diretamente. Se a estação usar outro programa/formato ou outro tipo de modem, confira sua saída antes de presumir compatibilidade.

Os atalhos `.bat` são para Windows. Em outro sistema com Python/Tk disponível, instale `requirements.txt` e execute `python main.py --receptor`.

## 3. Enviar o registro após encerrar

No **computador da bancada**:

1. Aguarde a confirmação do encerramento da aquisição.
2. Abra **Arquivos no ESP32…**.
3. Selecione o identificador do ensaio. A pasta salva no PC contém o mesmo identificador.
4. Clique **Enviar por LoRa**.
5. Acompanhe o progresso nos dois computadores.

No notebook receptor, a confirmação é **RECEBIDO E VERIFICADO**. O aplicativo reconstrói o arquivo, confere tamanho e integridade, salva um CSV e mostra a curva bruta.

**Envio concluído no ESP32 não confirma recebimento.** Se faltarem blocos, o receptor informa quantos faltam. Envie novamente o mesmo registro; a estação aproveita os blocos válidos já recebidos. Ela pode retomar após fechar e abrir o aplicativo, desde que use a mesma pasta de destino.

O envio é unidirecional, sem ACK pelo rádio. A solicitação de envio chega ao ESP32 por USB. O original não é apagado automaticamente. Durante a transferência, aguarde o fim ou use **Cancelar envio LoRa** antes de iniciar outro ensaio.

### O que é transmitido

O **arquivo bruto completo gravado no ESP32**: identificador, sequência das amostras, tempos, leituras do HX711, saturação e motivo de encerramento quando disponível.

O nome descritivo, as observações digitadas e a referência visual definida no PC permanecem no JSON da bancada; não fazem parte do arquivo interno nesta versão. O identificador comum permite associá-los.

LoRa transfere arquivos mais lentamente que USB. O tempo depende do tamanho e das perdas. Comece pelo ensaio curto de 10 s antes de transmitir registros longos.

## Onde ficam os dados

Na bancada: `%USERPROFILE%\TCC_Bancada\ensaios`, ou a pasta escolhida.

| Arquivo | Conteúdo |
|---|---|
| `usb.csv` | Amostras recebidas diretamente da bancada |
| `lora.csv` | Telemetria ao vivo opcional, se usada no mesmo PC |
| `ensaio.json` | Nome, notas, referência, portas, estado, motivo de parada, duração solicitada/medida e estatísticas |
| `eventos.jsonl` | Comandos e mensagens durante o ensaio |

No receptor: `%USERPROFILE%\TCC_Bancada\recebidos_lora`, ou a pasta escolhida.

| Arquivo | Conteúdo |
|---|---|
| `registro.log` | Cópia verificada do registro interno do ESP32 |
| `amostras.csv` | Amostras brutas, sem conversão para kg/N |
| `recepcao.json` | Identificação, integridade e progresso da transferência |
| `registro.log.partial` | Arquivo ainda em recepção; não considerar completo |

Mantenha os arquivos juntos para permitir retomada. Um arquivo pode chegar integralmente e conter uma aquisição original sem encerramento normal; o aplicativo diferencia essas situações.

## Limites desta etapa

- Referência sem carga não substitui calibração com uma referência física conhecida.
- Encerramento por botão, duração ou falha detectada; ainda não por retorno próximo de zero.
- USB e flash guardam as amostras efetivamente adquiridas. A taxa real ainda precisa ser medida.
- Falta de energia pode deixar registros incompletos. Não há apagamento automático na inicialização.
- A flash externa, os perfis de calibração e o controle pelo rádio ficam para etapas posteriores.
- A telemetria ao vivo permanece opcional. O fluxo principal é USB e envio posterior do registro.

Mais detalhes: `docs/transferencia_lora_v0_3.md` e `docs/validacao.md`.
