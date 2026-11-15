# Bancada de Empuxo V0.10 — Etapa 1: protocolo serial

Entrega de 14/11/2026, baseada no arquivo `Bancada_Empuxo_V0.9(2).ino` enviado pelo usuário. Esta etapa prepara a comunicação pela **serial USB**. A Interface Bancada Etapa 06 foi consultada como referência; nenhum código Python foi alterado ou integrado.

## Abrir e testar

1. Extraia o ZIP completo.
2. Abra `Bancada_Empuxo_V0_10/Bancada_Empuxo_V0_10.ino` na Arduino IDE.
3. Mantenha a seleção de placa, porta, partições e demais opções usadas para a V0.9. O nome da pasta coincide com o nome do sketch.
4. Use as bibliotecas HX711 (API de bogde/Bogdan Necula), LoRa (Sandeep Mistry) e ArduinoJson **7.x**. SPI e LittleFS pertencem ao pacote ESP32. Não substitua por bibliotecas homônimas com APIs diferentes.
5. Compile e envie para a placa. A compilação para ESP32 não foi executada nesta entrega; os testes locais estão descritos em `VALIDACAO.md`.
6. Abra o Monitor Serial em **115200 baud**, com final de linha **Nova linha**. Também são aceitos CR e CRLF. A opção **Sem final de linha** deixa o comando aguardando e não o executa.
7. Reinicie com a célula sem carga e espere a mensagem `PRONTO`. A tara inicial usa dez leituras e tem limite total de três segundos. Consulte `STATUS` para saber quais componentes inicializaram.

## Primeiro teste: somente serial, sem transmissão LoRa

Envie **uma linha de cada vez**. Não cole toda a sequência, pois ela iniciaria e encerraria a aquisição imediatamente.

| Ação no Monitor Serial | Resultado esperado |
| --- | --- |
| `AJUDA` | JSON com os comandos disponíveis |
| `STATUS` | Firmware `0.10`, estado `OCIOSO`, unidade `kgf`; confira `tara_pronta`, `hx711_respondendo` e `memoria_pronta` |
| `XYZ` | Erro `COMANDO_DESCONHECIDO`; nenhuma operação é iniciada |
| `INICIAR` | Resposta com `ok: true`, estado `ADQUIRINDO` e LED verde aceso |
| Aguarde alguns segundos e aplique uma pequena carga de teste | Linhas de tipo `amostra`, com tempo e empuxo calculado; amarelo apagado neste teste |
| `STATUS` | Estado `ADQUIRINDO` e contador de amostras crescente |
| `FINALIZAR` | Evento `ENSAIO_ENCERRADO`, resposta de sucesso e LED verde apagado |
| `STATUS` | Estado `OCIOSO` e quantidade de amostras gravadas nesta sessão |

**Armazenamento preservado da V0.9:** existe um único `/ensaio.bin` na flash interna. Cada `INICIAR` ou `INICIAR_LORA` aceito substitui o ensaio anterior. A política original `LittleFS.begin(true)` também foi mantida: falha de montagem pode levar a uma tentativa de formatação. Esta entrega não implementa histórico de múltiplos ensaios.

## O que mudou

- Entrada serial por linha completa: palavras, parâmetros inesperados e comandos concatenados não executam as letras individuais como ações. Os atalhos antigos continuam disponíveis, agora seguidos de Enter.
- Saída da aplicação em JSON, uma mensagem por linha. Foram acrescentados `STATUS`, `AJUDA`, confirmações e erros estruturados. A saída antiga `tempo_ms;leitura_bruta` foi substituída por mensagens `amostra` com `empuxo_kgf`.
- Comandos explícitos `LORA_LIGAR` e `LORA_DESLIGAR`, além da alternância `l` já existente. Todos os comandos continuam sendo recebidos pela serial USB.
- A tara automática no boot deixa de esperar indefinidamente. Se o HX711 parar de fornecer leituras por dois segundos, a aquisição é encerrada, a tara passa a exigir nova inicialização e um erro é emitido.
- Falhas de abertura, escrita, leitura e reposicionamento da flash são identificadas. Um registro incompleto é recusado no envio gravado.
- `INICIAR_LORA` valida a disponibilidade do rádio antes de abrir/substituir a gravação. Repetir o comando durante um ensaio não o reinicia nem troca seu modo.
- O verde indica aquisição. O amarelo acompanha o estado de transmissão local informado pelo rádio, inclusive um último pacote ainda em andamento após finalizar ou cancelar.

## kgf e calibração nesta etapa

A fórmula permanece:

`empuxo_kgf = (leitura do HX711 − tara interna) / fator_calibracao`

O coeficiente deve estar em **contagens por kgf**. O valor **1.0** foi preservado do arquivo original; ele permite testar o fluxo de dados, mas **não produz uma medição física calibrada em kgf**. A inicialização informa isso com o aviso `COEFICIENTE_PADRAO`.

Nesta etapa o coeficiente continua em `FATOR_CALIBRACAO_PADRAO`, no início do firmware. A aplicação do coeficiente recebido do computador será a etapa seguinte, assim como o comando de tara. Não há cadastro de perfis no ESP32, seletor de N/kgf ou comando para definir offset. A tara automática existente continua inteiramente interna ao ESP.

## Escopo desta entrega

| Recurso | Situação na V0.10 |
| --- | --- |
| Operação e diagnóstico pela serial USB | Implementados nesta etapa |
| Aquisição e gravação na flash interna | Mantidas, com diagnóstico de falhas |
| Saída serial de empuxo com campo em kgf | Implementada; depende de coeficiente calibrado para ter significado físico |
| Perfis no computador e envio do coeficiente selecionado ao ESP | Preparação futura; nenhum perfil armazenado no ESP |
| Tara por comando | Próxima etapa; nesta versão a tara ocorre somente ao inicializar |
| Acionamento do motor | Não implementado no V0.9 anexado nem nesta entrega; nenhum GPIO de ignição foi escolhido |
| Separação entre aquisição e motor | `INICIAR` faz apenas aquisição; `STATUS` informa `motor_implementado: false` |
| LoRa ao vivo e envio do ensaio gravado | Preservados, acionados pela serial |
| Erros para a futura janela de log | JSON com nível, origem, código e mensagem; encaminhamento ao Python será posterior |
| Histórico de vários ensaios, exclusão em lote e duração programada | Etapas futuras |

## Configuração preservada

| Item | Valor |
| --- | --- |
| HX711 DT / SCK | GPIO 33 / GPIO 32 |
| LoRa CS / RST / DIO0 | GPIO 21 / GPIO 14 / GPIO 26 |
| SPI SCK / MISO / MOSI | GPIO 18 / GPIO 19 / GPIO 23 |
| LED verde / amarelo | GPIO 4 / GPIO 2, ativos em nível alto |
| Serial | 115200 baud |
| Rádio | 915 MHz; SyncWord 145; potência configurada 2; SPI 1 MHz |
| Intervalo LoRa | 250 ms; envio assíncrono |
| Armazenamento | LittleFS da flash interna, `/ensaio.bin` |

O JSON enviado por LoRa continua com `tempo_ms`, `empuxo` e `fator_calibracao`. O layout da estrutura `Payload` foi mantido. O protocolo novo desta entrega se aplica à serial; não houve mudança no receptor ou no programa Python. Um arquivo binário antigo não contém identificação de unidade ou versão: o firmware não converte automaticamente ensaios gravados com outra convenção de calibração.

## Arquivos

- `Bancada_Empuxo_V0_10/`: sketch atual, em um único `.ino`.
- `COMANDOS.md`: protocolo completo, exemplos e catálogo de erros.
- `VALIDACAO.md`: testes executados e o que depende da placa.
- `historico/Bancada_Empuxo_V0_9/`: cópia exata do conteúdo do V0.9 anexado; abra-o separadamente para retornar à versão anterior.
- `validacao_nativa/`: fontes dos testes, simuladores e resultados para reprodução opcional no computador. Não copie esses arquivos para a pasta do sketch.

A V0.9 foi preservada; a próxima entrega deve avançar para V0.11. Primeiro valide esta etapa pela serial antes de acrescentar o recebimento do coeficiente e a tara por comando.
