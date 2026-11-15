# Bancada de Empuxo V0.16 — vários ensaios no LittleFS

Base V0.15. Somente firmware; pinagem, HX711, rádio e USB a **115200 baud** preservados. O protocolo de comandos continua **2** e a amostra USB/LoRa continua no formato **2**. O formato interno dos novos arquivos é **3**.

Abra **Bancada_Empuxo_V0_16/Bancada_Empuxo_V0_16.ino** na Arduino IDE. Mantenha **Armazenamento.h** e **ComandosArmazenamento.h** na mesma pasta do sketch. O primeiro contém os registros e sua leitura; o segundo, os comandos de catálogo. Não copie apenas o .ino.

Bibliotecas: HX711 de bogde/Bogdan Necula, LoRa de Sandeep Mistry, ArduinoJson 7.x e SPI/LittleFS do core ESP32. Use a placa, porta e partições que já funcionam na bancada. A interface Python e o receptor LoRa não foram alterados. Não há acionamento de motor.

## Mudanças desta etapa

- Cada novo ensaio cria `/ensaios/<ensaio_id>.ben`; não substitui os anteriores. Colisões de ID são verificadas antes da abertura. Não existe apagamento automático por falta de espaço.
- INICIAR e INICIAR_LORA aceitam nome, data, observações, identificação da célula e do perfil.
- O cabeçalho guarda o coeficiente realmente utilizado, a tara interna aplicada e kgf. A tara é somente informação de leitura, nunca um parâmetro recebido.
- LISTAR_ENSAIOS lista até quatro itens por página; CONSULTAR_ENSAIO informa detalhes; LER_AMOSTRAS recupera até quatro amostras por bloco.
- EXCLUIR_ENSAIOS exclui até quatro IDs selecionados com resultado individual; EXCLUIR_TODOS exige confirmação explícita e emite o resultado de cada arquivo.
- ESPACO informa total, usado, livre e margem reservada.
- Arquivos em gravação ou transmissão são protegidos. O último pacote LoRa ainda em TX também mantém a proteção.
- LittleFS inicia com **begin(false)**. Falha de montagem não provoca formatação: o ESP continua respondendo pela serial e informa o erro.
- Cabeçalho, amostras e encerramento possuem CRC32. Registros sem encerramento normal são incompletos; amostras íntegras podem ser recuperadas em blocos.

## Onde começar

**COMANDOS_E_TESTES.md** traz o roteiro para copiar pela serial, incluindo criar dois ensaios, reiniciar, listar, recuperar e excluir. **PROTOCOLO_USB.md** descreve os comandos e a paginação. **FORMATO_E_RECUPERACAO.md** explica metadados, limites, formatos antigos e falhas. **VALIDACAO.md** separa os testes realizados daqueles pendentes no hardware.

O armazenamento continua sendo **a flash interna do ESP32 via LittleFS**. Nenhum driver para memória SPI externa foi presumido ou acrescentado.

## Seus registros anteriores

O `/ensaio.bin` antigo permanece intacto. Quando existe, aparece no catálogo como **LEGADO_ENSAIO_BIN**. A versão 0.15 é reconhecida como formato 2; registros anteriores sem cabeçalho versionado são identificados como legado sem versão. Eles podem ser consultados quanto a existência/tamanho/formato e excluídos por comando explícito, mas não são reinterpretados como registros 3.

Não houve migração automática. Para recuperar medições de arquivos legados, use uma ferramenta/versão compatível antes de excluí-los; não inicie um novo ensaio em firmware antigo que ainda substitua `/ensaio.bin`. Os novos arquivos `.ben` são independentes dele. Outros arquivos fora do catálogo não são removidos por EXCLUIR_TODOS.

Uma partição sem LittleFS montável precisará de preparação explícita fora do fluxo normal. Esta entrega não oferece comando de formatação. EXCLUIR_TODOS remove arquivos de ensaio, não formata a partição.

## Validação

**118/118 cenários nativos aprovados**, incluindo preservação de vários arquivos entre dois processos de teste separados, com um ensaio interrompido. Verificações com sanitizadores e com cabeçalhos oficiais HX711/LoRa também realizadas. Isso não substitui compilação completa para ESP32, upload ou ensaio físico; esses itens permanecem pendentes na sua placa.

O pacote não inclui versões anteriores. `validacao_nativa/` guarda fontes e resultados das simulações e não é necessária para carregar o firmware.
