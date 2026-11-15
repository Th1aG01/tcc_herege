# Bancada de Empuxo V0.13 — protocolo USB JSON

Base: V0.12 entregue neste chat. Esta versão prepara exclusivamente o firmware; não modifica a interface Python. O baud permanece **115200**. Não acompanha pasta de histórico.

## Carregar e começar

1. Extraia o ZIP e abra `Bancada_Empuxo_V0_13/Bancada_Empuxo_V0_13.ino`.
2. Mantenha a placa, porta, partições e bibliotecas que já utiliza: HX711 de bogde, LoRa de Sandeep Mistry e ArduinoJson 7.x; SPI e LittleFS do core ESP32.
3. Compile na Arduino IDE e faça o upload. A compilação completa para ESP32 e o hardware ainda precisam ser validados por você; veja `VALIDACAO.md`.
4. Abra o Monitor Serial em **115200**, com **Nova linha** ou **Ambos NL e CR**.
5. Envie exatamente esta linha:

```json
{"protocolo":2,"tipo":"comando","id":0,"comando":"IDENTIFICAR"}
```

A resposta informa `dispositivo`, `firmware: "0.13"`, `sessao`, `maior_id` e limites do protocolo. Copie o valor de **16 caracteres** de `sessao` para os próximos comandos. A sessão muda ao reiniciar; descubra-a novamente após cada reset/upload.

Exemplo de consulta: **substitua `0123456789ABCDEF` pelo valor recebido**, e use um ID maior que `maior_id`:

```json
{"protocolo":2,"tipo":"comando","sessao":"0123456789ABCDEF","id":1,"comando":"STATUS"}
```

Espere `sensor.tara_pronta: true` antes de iniciar. A tara inicial agora coleta uma leitura por passagem do loop, permitindo consultas USB enquanto está em andamento. Não foi acrescentado comando de tara nesta etapa.

## O que mudou

- Apenas objetos JSON completos por linha são aceitos. Atalhos como `i`, `a` e `f`, palavras soltas e comandos concatenados deixam de ser comandos válidos.
- Protocolo incrementado para **2**, firmware para **0.13**. O protocolo 1 foi substituído, sem modo legado automático.
- Recepção de fragmentos, limite de 512 bytes, timeout de um segundo entre fragmentos e descarte da linha inteira em erro. O parser rejeita chaves duplicadas, texto adicional e objetos concatenados.
- Identificação estável pelo eFuse do ESP32 e uma identificação de sessão gerada a cada boot.
- IDs crescentes, respostas retidas e rejeição de IDs antigos para impedir execução repetida.
- Respostas de aceitação e eventos terminais distintos para aquisição e envio gravado.
- Consulta de aquisição, sensor, calibração, flash e rádio. Erros incluem código, origem e descrição.
- Saída serial em fila, drenada somente conforme o espaço de escrita da UART, sem `Serial.flush()` ou espera pela linha completa. Sob congestionamento, amostras USB podem ser descartadas com contagem explícita; a aquisição continua gravando na flash enquanto ela estiver disponível.
- Timeout local de cinco segundos para TX_DONE: em falha, desabilita o rádio até reiniciar e conclui o envio gravado com falha. A aquisição, se estiver ativa, continua.

## O que foi preservado

| Item | Configuração |
| --- | --- |
| HX711 DT / SCK | GPIO 33 / 32 |
| LoRa CS / RST / DIO0 | GPIO 21 / 14 / 26 |
| SPI SCK / MISO / MOSI | GPIO 18 / 19 / 23 |
| LEDs verde / amarelo | GPIO 4 / 2 |
| Rádio | 915 MHz; SyncWord 145; potência 2; SPI 1 MHz; intervalo 250 ms |
| Transmissão | Assíncrona, com callback público `onTxDone`; DIO0 precisa estar ligado ao GPIO 26 |
| Flash | LittleFS interno; um arquivo `/ensaio.bin`; início aceito substitui o ensaio anterior |
| Medição | Leitura corrigida pela tara / coeficiente; unidade prevista kgf |

O coeficiente **1.0** continua preservado e não representa calibração física validada. Perfis ficam no computador; esta entrega ainda não adiciona recebimento de coeficiente, ajuste de offset, ignição ou histórico de múltiplos ensaios. Iniciar aquisição não aciona motor.

O layout do `Payload` e os três campos do JSON LoRa (`tempo_ms`, `empuxo`, `fator_calibracao`) permanecem iguais. O novo protocolo se aplica à USB. A política `LittleFS.begin(true)` foi preservada: uma falha de montagem pode provocar tentativa de formatação.

Veja `PROTOCOLO_USB.md` para exemplos completos, regras de repetição e mensagens de erro. `validacao_nativa/` contém testes opcionais e seus resultados; não é necessária para carregar o sketch.
