# Bancada de Empuxo V0.15 — ciclo de aquisição

Base: V0.14. Somente firmware; interface Python não foi alterada. USB a **115200 baud**, protocolo de comandos JSON **2**, pinagem e configurações de rádio preservados. Sem pasta de histórico.

Abra **Bancada_Empuxo_V0_15/Bancada_Empuxo_V0_15.ino** na Arduino IDE. Mantenha a placa, porta e partições que você já utiliza. Bibliotecas: HX711 de bogde/Bogdan Necula, LoRa de Sandeep Mistry, ArduinoJson 7.x e SPI/LittleFS do core ESP32. Os demais arquivos são documentação e testes; não devem ser copiados para a pasta do sketch.

## O que mudou

- INICIAR valida sensor recente, calibração explícita, tara e armazenamento antes de preparar a aquisição.
- Cada ensaio recebe ID próprio: sessão do boot + contador. O ID da solicitação permanece separado.
- A resposta `aceito` informa a preparação. O evento `AQUISICAO_INICIADA` só ocorre após gravar a primeira medição válida; nesse momento acende o LED verde.
- Cada amostra inclui ID do ensaio, índice começando em 1, tempo relativo em ms começando em 0 e empuxo calibrado em kgf.
- USB, gravação e LoRa usam a mesma medição. O LoRa ao vivo seleciona a última amostra ainda não enviada a cada oportunidade de 250 ms; não controla a aquisição.
- Ao parar, o arquivo recebe um rodapé com contagem, duração e motivo. O firmware faz flush, fecha, reabre e verifica o fechamento antes de informar conclusão normal.
- Falhas de sensor, cálculo, espaço ou gravação resultam em ensaio falho. Arquivos sem fechamento válido são incompletos e não entram no envio normal.
- STATUS informa o ensaio atual ou o último resultado, inclusive depois de reconectar a serial. A ausência do computador não é condição de parada.

## Teste inicial

Use o Monitor Serial em **115200** e **Nova linha**. Siga **CICLO_AQUISICAO.md**, que traz os JSON para copiar, resultados esperados, falhas e reconexão. **CALIBRACAO_E_TARA.md** explica os coeficientes e a tara. **PROTOCOLO_USB.md** descreve todos os comandos, campos e repetições.

Fluxo: IDENTIFICAR → aguardar tara inicial → CONFIGURAR_CALIBRACAO → TARA → STATUS → INICIAR → aguardar AQUISICAO_INICIADA → amostras → FINALIZAR → conferir ENSAIO_ENCERRADO.

Nenhum desses comandos inicia motor. A entrega não implementa acionamento de motor.

## Armazenamento e compatibilidade

Continua sendo **LittleFS na flash interna do ESP32**, com **um ensaio por vez**. Preparar um novo ensaio abre `/ensaio.bin` para substituição do anterior; falhas depois dessa abertura também podem deixar o arquivo anterior indisponível. Não foi acrescentado driver para flash SPI externa nem um catálogo de ensaios.

**O formato do arquivo e das amostras mudou nesta versão.** IDENTIFICAR informa `formato_amostra: 2` e `formato_gravacao: 2`; o envelope de comandos continua no protocolo 2. No formato novo, `ensaio_id` é string, não o número do pedido. O LoRa agora usa o mesmo JSON da amostra USB (`empuxo_kgf`), substituindo os nomes antigos. Um receptor antigo precisa ser adaptado depois; ele não foi alterado nesta etapa.

Arquivos das versões antigas não são reinterpretados como formato novo; ENVIAR os rejeita. Obtenha os dados antigos antes de iniciar outro ensaio. A política já existente `LittleFS.begin(true)` foi mantida: uma falha de montagem pode ocasionar formatação automática. Um reset não retoma a aquisição: o firmware inspeciona o arquivo e informa o resultado recuperável, mas exige nova configuração e tara para iniciar outro ensaio.

## Validação

**78/78 cenários nativos aprovados**, com verificações de memória e comportamento indefinido. Foram conferidos também os cabeçalhos oficiais de LoRa e HX711. Isso não é uma compilação completa para ESP32 nem um teste físico. Compilação na Arduino IDE, upload, sensor, taxa real, flash e rádio permanecem pendentes na sua bancada; detalhes em **VALIDACAO.md**.

`validacao_nativa/` contém fontes e resultados das simulações; não é necessária para carregar o firmware. `VERSAO.txt` identifica a entrega; `SHA256SUMS.txt` permite conferir a integridade dos arquivos.
