# Bancada de Empuxo V0.14 — calibração e tara por USB

Base: V0.13. Mantém protocolo JSON **2**, USB em **115200 baud**, pinagem, parâmetros LoRa, LEDs e organização em um único sketch. Sem alterações na interface Python e sem pasta de histórico.

Abra `Bancada_Empuxo_V0_14/Bancada_Empuxo_V0_14.ino`, mantendo a seleção de placa, porta, partições e bibliotecas já utilizadas. Bibliotecas: HX711 de bogde/Bogdan Necula, LoRa de Sandeep Mistry, ArduinoJson 7.x e SPI/LittleFS do core ESP32. Compile e envie pela Arduino IDE. Os testes locais estão separados dos testes pendentes na placa em `VALIDACAO.md`.

## Fluxo desta versão

1. Descobrir a sessão com `IDENTIFICAR`.
2. Aguardar a tara inicial terminar ou informar falha; USB continua acessível durante a espera.
3. Enviar `CONFIGURAR_CALIBRACAO` com o coeficiente do perfil do computador, em contagens/kgf.
4. Sem carga, enviar `TARA` e aguardar o evento `TARA_CONCLUIDA` da mesma solicitação.
5. Conferir `CONSULTAR_CALIBRACAO` ou `STATUS`: calibração válida, tara pronta e `pronta_para_uso: true`.
6. Iniciar o ensaio com `INICIAR` ou `INICIAR_LORA`.

**O valor inicial 1.0 não libera mais o ensaio.** É necessário configurar explicitamente o coeficiente. Um valor 1.0 enviado explicitamente é aceito, como qualquer número finito e não zero; isso não comprova a calibração física da célula.

Cada nova configuração aceita invalida a tara, mesmo que o número seja igual ao anterior: um perfil pode representar outra célula. A repetição da mesma solicitação com o mesmo ID não executa a alteração novamente e não invalida uma tara feita depois.

O coeficiente e o estado de calibração ficam em RAM. Após reiniciar, envie novamente o perfil e faça tara. Nenhum perfil, offset externo ou seletor de unidade foi acrescentado ao ESP.

## Novos comandos

| Comando | Efeito |
| --- | --- |
| `CONFIGURAR_CALIBRACAO` | Recebe o campo numérico `coeficiente`; aceita sinal positivo/negativo, decimais e notação científica |
| `CONSULTAR_CALIBRACAO` | Informa coeficiente, origem, validade e prontidão da tara/calibração |
| `TARA` | Operação assíncrona: dez leituras brutas, limite total de 3000 ms |

Alterar coeficiente ou executar tara durante aquisição gera `ENSAIO_ATIVO`, sem mudar os valores. Outra tara ou alteração de coeficiente durante uma tara em andamento gera `TARA_EM_ANDAMENTO`. Consultas continuam disponíveis. Tara e transmissão de um arquivo gravado podem avançar simultaneamente: cada operação mantém seu próprio ID, e o arquivo usa os coeficientes gravados em suas amostras.

## Cálculo e recuperação

O firmware calcula `empuxo_kgf = celula.get_value() / coeficiente`. Na biblioteca HX711 de bogde, `get_value()` já subtrai o offset interno, mas não divide pelo fator de escala. A tara nova usa `read()` bruto e aplica sua média com `set_offset()`. Não usa `get_units()`, não aplica o coeficiente com `set_scale()` e não subtrai o offset de novo.

Se não houver dez leituras em três segundos, a tara falha e continua inválida. A serial permanece responsiva. Após conferir/reconectar o sensor, envie `TARA` com um novo ID para tentar novamente; não precisa reiniciar. Retorno de leituras sozinho não torna a tara válida.

Veja **CALIBRACAO_E_TARA.md** para copiar os comandos de teste e **PROTOCOLO_USB.md** para o transporte e a proteção contra repetições. Um novo ensaio aceito continua substituindo o arquivo anterior, como nas versões anteriores. A política de montagem `LittleFS.begin(true)` e o JSON LoRa não foram alterados.
