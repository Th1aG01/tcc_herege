# Auditoria técnica — firmware V0.17 e interface Etapa 06

**Data:** 15/11/2026, referência America/Sao_Paulo.  
**Projeto:** bancada de empuxo BRAVO / Univap.  
**Escopo:** diagnóstico somente leitura conforme `PROMPT_AUDITORIA_WORK_TCC(1).md`, com confronto do checkpoint de 15/11/2026. Nenhum código original, configuração, protocolo ou documento do projeto foi alterado. Scripts e resultados desta auditoria são artefatos separados.

## 1. Parecer para o chat de coordenação

**O firmware tem uma base implementada e testável; a interface ainda é um front-end demonstrativo, sem integração com o ESP32. O conjunto não está pronto para uso como sistema integrado.** Não há motivo demonstrado para reescrever o firmware inteiro. A próxima etapa deve fechar a decisão de armazenamento e obter uma compilação real para a placa, seguida de integração USB em passos pequenos, com a ignição fisicamente desabilitada.

Resultados desta rodada:

- **141/141 cenários nativos do firmware aprovados**, recompilados nesta auditoria com g++ e AddressSanitizer/UndefinedBehaviorSanitizer. Não foram apenas lidos os resultados fornecidos no ZIP.
- **24/24 verificações adicionais da camada Python aprovadas**, abrangendo sintaxe/importação dos 12 módulos, persistência de perfis, conversão de unidades, rejeições, backup e logs. Não equivalem a 24 testes visuais ou a testes de comunicação.
- **Uma inconsistência de validação reproduzida:** cadastro local de perfis aceita chaves JSON duplicadas, inclusive `coeficiente`, enquanto a importação rejeita o mesmo documento.
- **Divergência arquitetural confirmada:** armazenamento implementado em **LittleFS na Flash interna do ESP32**. A Flash SPI externa de 16 Mbits não possui integração neste código.
- **Ausências confirmadas:** transporte USB na GUI, recepção de amostras reais, salvamento/exportação de ensaios, receptor BT/1 entregue e acionamento de motor.
- **Contradição documental confirmada:** `COMANDOS_E_TESTES.md:102` ainda descreve escolha implícita do último ensaio em `ENVIAR`; o código e o protocolo da V0.17 exigem ID explícito.
- Não houve compilação completa para ESP32, upload, ensaio físico, medição de latência RF ou validação da PCB. Não se encontrou plataforma ESP32 instalada no ambiente de compilação consultado.

Não foi reproduzido um defeito P0 de execução na base simulada. Isso **não libera a operação com motor**: circuito, intertravamentos e condições físicas não foram fornecidos para validação, e a função de ignição sequer está implementada.

## 2. Baseline, inventário e dependências

### 2.1 Identificação exata

| Entrada | Versão verificada | Conteúdo |
|---|---|---|
| `Bancada_Empuxo_V0_17_Transferencia_LoRa(1).zip` | Firmware 0.17; USB 2; amostra 2; gravação 3; transporte gravado BT/1 | 164 arquivos; 4 fontes principais, documentação, mocks, testes e resultados |
| `Interface_Bancada_Etapa_06(1).zip` | Interface 06, 14/11/2026 | 16 arquivos; 12 módulos Python, JSON de demonstração, BAT, README e versão |
| `CHECKPOINT_TCC_2026-11-15(1).md` | Checkpoint 01 | Contexto e hipóteses anteriores à inspeção |
| `PROMPT_AUDITORIA_WORK_TCC(1).md` | Auditoria somente leitura | Critérios desta entrega |

SHA-256 dos ZIPs originais:

```text
Firmware: 8e3f904c3f3a25881d212b8fa7d5f427617c3544565aab7e0bd9b561b71b5f57
Interface: f1c946baf5298d609d12420f23db0e0bf2be6c3ad8cdd485b4b81a9fe1ad8853
```

O manifesto `SHA256SUMS.txt` do firmware passou. Os arquivos extraídos da baseline foram comparados byte a byte com os dois ZIPs após as verificações e permaneceram intactos. A execução do runner foi feita em cópia separada, pois ele regrava os JSONL e `resultados.json`.

Referências de linhas abaixo são relativas aos arquivos dentro dos ZIPs: **F** = `Bancada_Empuxo_V0_17/Bancada_Empuxo_V0_17.ino`; cabeçalhos ficam nessa mesma pasta. **G** = `Interface_Bancada_Etapa_06/`. O anexo `inventario.json` enumera todos os 180 arquivos, tamanho, linhas e SHA-256.

### 2.2 Firmware: 1.908 linhas nos quatro fontes principais

| Arquivo | Linhas | Papel |
|---|---:|---|
| `Bancada_Empuxo_V0_17.ino` | 1.226 | Inicialização, estado global, USB/JSON, cache/deduplicação, calibração/tara, aquisição, ações e loop |
| `Armazenamento.h` | 207 | Layout binário, CRC, identificação, inspeção e paginação de arquivos |
| `ComandosArmazenamento.h` | 251 | Listagem, consulta, leitura em blocos, exclusões e espaço |
| `TransferenciaLoRa.h` | 224 | Máquina de estados BT/1, quadros, ACK/PEDIR/CONFIRMAR, timeouts e cancelamento |
| `validacao_nativa/test_firmware.cpp` | Inventário anexo | Harness e asserções dos cenários; inclui os fontes reais |
| `validacao_nativa/run_tests.py` | Inventário anexo | Executa 141 cenários; gera resultados/JSONL; disco temporário para reboot entre processos |
| `validacao_nativa/mocks/Arduino.h`, `HX711.h`, `LittleFS.h`, `LoRa.h`, `SPI.h`, `esp_random.h`, `mock_hw.h` | Inventário anexo | Hardware, filesystem, relógio e UART simulados |
| `validacao_nativa/*.jsonl`, `resultados.json` | Inventário anexo | Evidências fornecidas da entrega anterior; distinguidas das novas execuções |
| `PROTOCOLO_USB.md`, `PROTOCOLO_LORA.md` | — | Contratos de comunicação |
| `FORMATO_E_RECUPERACAO.md` | — | Layout/persistência e recuperação |
| `COMANDOS_E_TESTES.md`, `TESTES_LORA.md` | — | Roteiros manuais |
| `VALIDACAO.md`, `LEIA_ME.md`, `VERSAO.txt`, `SHA256SUMS.txt` | — | Limites, dependências, identificação e integridade |

Dependências declaradas: Arduino/core ESP32, SPI e LittleFS do core; HX711 de bogde; LoRa de Sandeep Mistry; ArduinoJson 7.x. Na suíte reproduzida, ArduinoJson **7.4.2 real**; demais APIs embarcadas simuladas. A entrega relata checagem anterior de APIs LoRa 0.8.0/HX711, mas não fornece uma matriz travada de placa/core/partição/HX711. Essa checagem anterior não foi apresentada como nova execução desta auditoria.

### 2.3 Interface: 2.599 linhas Python

| Arquivo | Linhas | Papel e estado |
|---|---:|---|
| `main.py` | 840 | Janela, temas, controles, cronômetro demonstrativo, perfil selecionado e encaminhamento de erros |
| `calibracao.py` | 422 | Editor/lista de perfis, importar/exportar/excluir; não calibra sensor |
| `perfis_calibracao.py` | 313 | Validação, conversão de legado, persistência atômica e conflito sequencial de edição |
| `memoria.py` | 278 | Histórico demonstrativo, seleção, detalhes e exclusão da sessão |
| `log_erros.py` | 197 | Log persistente, lock, janela, copiar e exportar TXT |
| `calendario.py` | 137 | Calendário e seleção de `date` |
| `componentes.py` | 95 | Painel rolável e foco |
| `confirmacao_motor.py` | 86 | Checkbox, confirmar/cancelar e prevenção de confirmação repetida; sem comando físico |
| `dados_memoria.py` | 73 | `TesteSalvo`, conversão/validação de amostras e memória fictícia |
| `grafico.py` | 69 | Curva histórica em `tk.Canvas` |
| `dialogos_memoria.py` | 68 | Diálogos de confirmação e prévias |
| `unidades.py` | 21 | kgf fixo e conversões N↔kgf pertinentes |
| `dados_demonstracao.json` | — | Quatro ensaios fictícios |
| `INICIAR.bat` | — | Iniciador Windows com `py -3` ou `python` |
| `LEIA_ME.md`, `VERSAO.txt` | — | Escopo explicitamente sem hardware |

A GUI usa biblioteca padrão/Tkinter, sem Matplotlib, pandas, pyserial ou sockets. A ausência de `requirements.txt` não impede esta versão: não há dependências pip declaradas ou importadas. Quando o transporte real for adicionado, registrar suas dependências. Não há pasta de imagens externa necessária: ícone do calendário é desenhado pelo código.

### 2.4 Materiais adicionais consultados e limites

Foi consultado o texto extraído atual de `TG2_Texto_Base.docx`, versão 23, e de `TCC-v0.03_valdir20251119.docx`, para comparar as seções pertinentes. Não se editaram esses documentos, não se auditaram suas referências bibliográficas nem o layout do Word.

Não foram disponibilizados esquema elétrico/PCB atual, identificação física exata do chip externo, firmware receptor, logs físicos da V0.17, commit/tag ou repositório Git acessível. O ZIP contém base lógica herdada da V0.16, não a versão anterior integral nem histórico Git. Alegações de igualdade com V0.16/V0.9 não puderam ser verificadas por diff contra essas versões.

## 3. Arquitetura efetivamente encontrada

| Componente | Responsabilidade existente | Fronteira ausente/pendente |
|---|---|---|
| ESP32 | Ler HX711, aplicar tara/fator, registrar cada leitura aceita, interpretar USB, fornecer catálogo e transmitir LoRa | Flash SPI externa; comando de ignição; comprovação física |
| GUI | Formular dados, gerenciar perfis, mostrar exemplos, logs e estados locais | Conexão COM, sessão/IDs, comandos reais, recepção de amostras e persistência de ensaios |
| Receptor LoRa | Especificação BT/1 e simulação no teste | Firmware instalável, ponte receptor→PC e persistência/ACK reais |

A comunicação de **comando** da bancada nesta versão é USB. O firmware não implementa recebimento de comandos operacionais por LoRa: o RX de rádio atende os controles BT/1. Portanto, colocar apenas o receptor no notebook não oferece automaticamente controle remoto de INICIAR/FINALIZAR. Essa fronteira deve constar da arquitetura, especialmente quando se discutir a distância entre PC e bancada.

O loop é cooperativo: processa USB, tara, timeout/LEDs, uma leitura do sensor, telemetria, transferência e catálogo. Há limites de trabalho em RX/TX e catálogo, mas `LittleFS.open/write/flush/usedBytes`, leituras e exclusões são síncronos. “Cooperativo” não significa latência física garantida.

### Pinagem e rádio extraídos do código

| Função | Valor na V0.17 | Evidência |
|---|---|---|
| HX711 DT / SCK | GPIO 33 / 32 | F:20–21 |
| LoRa CS / RST / DIO0 | GPIO 21 / 14 / 26 | F:22–24 |
| SPI SCK / MISO / MOSI | GPIO 18 / 19 / 23 | F:1117 |
| LED execução / LoRa | GPIO 4 / 2 | F:25–26 |
| Clock SPI do LoRa | 1 MHz | F:1119 |
| Frequência / SyncWord / potência | 915 MHz / 145 decimal / 2 dBm | F:1120–1123 |
| Intervalo de telemetria | 250 ms entre oportunidades de envio | F:27, 1215–1220 |
| SF/BW/CR/preâmbulo/CRC físico | Não fixados explicitamente pelo sketch | `setup`; dependem da biblioteca utilizada |
| Flash externa / ignição | Nenhum CS/driver ou pino de ignição implementado | Conjunto dos quatro fontes |

Não há duplicação de GPIO entre as funções declaradas nessa tabela. Isso não prova compatibilidade com a PCB real. Não usar CS=5/DIO0=15 de conversas antigas para substituir o que o código declara.

## 4. Estado das funcionalidades

“Implementado” significa presente no código; não significa validado fisicamente.

| Função | Estado | Evidência/limite |
|---|---|---|
| JSON USB 115200, sessão, IDs, validação e deduplicação | Implementado no ESP | F:110–219, 917–1096; testes nativos |
| Cliente USB na GUI | Ausente | Não há transporte ou parser nos 12 módulos |
| Aquisição HX711 calibrada em kgf | Implementado no ESP | F:1159–1206; desempenho físico não verificável |
| Tara assíncrona e bloqueio por ausência de calibração | Implementado | F:396–462, 556–585 |
| Determinação automática do coeficiente com carga de referência | Ausente | GUI cadastra coeficiente informado |
| Perfis persistentes locais e conversão de legado | Implementado | `perfis_calibracao.py`; verificações Python |
| Aplicar perfil ao ESP e confirmar configuração | Ausente | `main.py:456–462` é seleção local |
| Nome/data/observações de ensaio | Parcialmente implementado | Formulário local e campos no ESP; sem transporte; limites divergem |
| Arquivos separados, CRC, catálogo e recuperação USB | Implementado no ESP | Cabeçalhos e testes de corrupção/reboot |
| Flash SPI externa de 16 Mbits | Ausente | LittleFS interno em F:1110 |
| Robustez a corte físico de energia | Não verificável | Simulação não reproduz persistência elétrica/cache real |
| LoRa ao vivo | Implementado no ESP | JSON sem ACK; transmite seleção das últimas amostras |
| Recuperação LoRa BT/1 com ACK/reparo | Implementado no transmissor | 23 cenários específicos + regressões; receptor real ausente |
| Transferência LoRa ponta a ponta | Parcialmente implementado | Não há receptor/ponte GUI entregue |
| LEDs | Implementado no ESP | Verde na aquisição efetiva; amarelo em TX local; físico pendente |
| Temas, layout, calendário, log e perfis | Implementado na GUI | Inspeção e testes de camada lógica; visual não reexecutado |
| Iniciar/parar/cronômetro | Parcialmente implementado | ESP possui ações reais; GUI muda apenas estado local |
| Gráfico ao vivo | Ausente | `main.py:284–293` é área vazia |
| Histórico com gráficos/tabela | Parcialmente implementado | Quatro exemplos em kgf; curva em Canvas |
| Salvar/abrir ensaio real; CSV/Excel | Ausente | Salvar é prévia; abrir refere-se aos exemplos |
| Excluir ensaios | Parcialmente implementado | Real no ESP; GUI exclui apenas exemplos em RAM |
| Apagar todos os ensaios persistentes pela GUI | Ausente | Selecionar todos os exemplos não equivale a EXCLUIR_TODOS |
| Ignição e intertravamentos locais do ESP | Ausente | Checkbox demonstrativo, nenhum comando/pino de motor |
| Média/impulso e relatório de ensaio real | Ausente | Há propriedade `pico` no modelo de exemplos, sem análise operacional |
| Watchdog explícito da aplicação, média móvel/rejeição de picos | Ausente | Timeouts existem; não confundir com watchdog/filtros implementados |

## 5. Matriz GUI ↔ ESP ↔ contrato

**Nenhuma linha abaixo representa comando já enviado pela GUI.** A coluna de correspondência identifica o contrato a usar futuramente, preservando a V0.17.

Envelope normal: `{"protocolo":2,"tipo":"comando","sessao":"<16 caracteres>","id":<inteiro crescente>,"comando":"<nome>",...}` + LF. Descoberta: ID 0, IDENTIFICAR, sem sessão. RX máximo 512 bytes antes de LF; JSON plano, campos extras rejeitados. Fases e erros precisam ser interpretados, não apenas `ok`.

| Controle/fluxo da GUI atual | Comando ESP correspondente | Parâmetros/formato | Resposta/estado esperado | Erros/atenções |
|---|---|---|---|---|
| Indicador USB, sem botão/configuração COM funcional | IDENTIFICAR → STATUS | Descoberta especial; depois sessão e ID novo | Identidade, maior_id, estado e subsistemas | SESSAO_INVALIDA, PROTOCOLO_INVALIDO; abrir COM não comprova identidade |
| Carregar perfil (`main.py:456`) | CONFIGURAR_CALIBRACAO; CONSULTAR_CALIBRACAO | coeficiente número finito ≠0 em contagens/kgf | concluido; configurada; tara invalidada | ENSAIO_ATIVO, TARA_EM_ANDAMENTO, COEFICIENTE_INVALIDO |
| Tara (`main.py:391`) | TARA | Sem offset/unidade | aceito → TARA_CONCLUIDA ou TARA_FALHOU | Dez leituras em 3 s; não iniciar só porque aceitou |
| Iniciar teste (`main.py:495`) | INICIAR | nome/data/observacoes/celula_id/perfil_id opcionais | aceito/PREPARANDO → AQUISICAO_INICIADA; ID persistente | Calibração/tara/sensor/Flash; envio/catalogação podem bloquear |
| Iniciar com telemetria, ainda sem controle próprio na GUI | INICIAR_LORA | Mesmos metadados | Mesmo fluxo, LoRa ao vivo habilitado | LORA_INDISPONIVEL impede esse início; INICIAR continua alternativa |
| Parar (`main.py:510`) | FINALIZAR | Sem parâmetros | Resposta do pedido; ENSAIO_ENCERRADO ligado ao ID de INICIAR | FLASH_FECHAMENTO; conferir resultado/contagem, não apenas resposta |
| Cronômetro e futuro gráfico | Amostra espontânea + STATUS/eventos | `ensaio_id`, `indice`, `tempo_ms`, `empuxo_kgf` | Índice inicia em 1; tempo zero na primeira leitura | Não usar perf_counter do clique como tempo de aquisição; detectar lacunas |
| Nome/data/observações/calendário | Campos de INICIAR | 48/10/128 bytes UTF-8; data YYYY-MM-DD | Metadados no cabeçalho e consulta | Campo visual é DD/MM/YYYY; usar `data_teste.isoformat()`; não truncar |
| Perfil/célula | Campos de INICIAR | `perfil_id` e `celula_id` até 32 bytes | Identificação, não aplicação implícita do fator | UUID hex novo tem 32 caracteres; descrição livre de célula pode exceder |
| Memória de testes (`memoria.py:69`) | LISTAR_ENSAIOS | limite 1–4; apos+revisao na continuação | aceito → LISTA_ENSAIOS; mais/proximo | CATALOGO_ALTERADO requer nova listagem; não presumir ordem por data |
| Abrir gráfico/detalhes (`memoria.py:126,229`) | CONSULTAR_ENSAIO + LER_AMOSTRAS | ensaio_id; inicio≥1; limite 1–4 | Metadados e blocos com indice/valida/tempo_ms/empuxo_kgf | ENSAIO_PROTEGIDO, FORMATO_NAO_SUPORTADO; `ok` do bloco não valida cada amostra |
| Salvar teste (`main.py:427`) | Recuperação acima + gravação no PC | Não há comando SALVAR necessário para gravar no ESP | ESP já grava durante aquisição; cópia PC deve ser confirmada | Hoje não salva; não apagar original após exportação automaticamente |
| Transmitir teste (`main.py:432`, `memoria.py:162`) | ENVIAR | ensaio_id obrigatório; um arquivo por transferência | aceito → progresso → ENVIO_LOCAL_CONCLUIDO → ENVIO_CONFIRMADO | Sem receptor: ENVIO_SEM_CONFIRMACAO; lote da GUI exige fila sequencial |
| Progresso LoRa, controle ainda ausente | CONSULTAR_ENVIO | transferencia_id opcional; ID de pedido novo por consulta | Snapshot atual/último | Repetir ID retorna snapshot antigo |
| Cancelamento LoRa, controle ainda ausente | CANCELAR | transferencia_id opcional recomendado | ENVIO_CANCELADO + resposta do cancelamento | Cancela envio local; não para aquisição nem confirma cancelamento remoto |
| LoRa ao vivo, controles ainda ausentes | LORA_LIGAR / DESLIGAR / ALTERNAR | Sem parâmetros | Estado do rádio e telemetria | LIGAR requer ensaio ativo; DESLIGAR não é CANCELAR |
| Excluir selecionados (`memoria.py:131`) | EXCLUIR_ENSAIOS | 1–4 IDs distintos | Resultados por arquivo; todos_excluidos | ENSAIO_PROTEGIDO, FLASH_EXCLUSAO; exclusão parcial possível |
| Selecionar todos, hoje apenas exemplos | EXCLUIR_TODOS ou lotes explicitamente selecionados | confirmacao exatamente EXCLUIR_TODOS para exclusão global | aceito → resultados → término | “Todos” deve distinguir seleção visível, catálogo completo e exemplos |
| Espaço, sem indicador atual | ESPACO | Sem parâmetros | bytes total/usado/livre/reserva; backend | FLASH_INDISPONIVEL/FLASH_ESPACO |
| Log (`main.py:147`) | Erros/avisos/eventos do ESP e erros do transporte | codigo/origem/descricao/fase/id/sessao | Registrar contexto e correlação | Não converter todo evento em erro; log atual só conhece eventos locais |
| Iniciar motor (`main.py:540–548`) | **Não existe** | Nenhum | Apenas mensagem demonstrativa | Checkbox não é intertravamento da bancada |
| Limpar gráfico, temas, calendário, exportar perfil/log | Nenhum | Operações locais | Efeito visual/arquivo local | Limpar visualização não deve excluir dados adquiridos |
| Diagnóstico técnico | AJUDA | Sem parâmetros | Lista de comandos | Sem controle dedicado na GUI |

No receptor futuro, separar pacotes `{...}` de telemetria e `BT` binários. O parser USB da bancada não recebe BT/1. Também será necessário definir o protocolo **receptor→PC**, ausente nesta entrega.

## 6. Achados priorizados

As prioridades abaixo classificam impacto, não a ordem histórica de desenvolvimento. Lacuna intencional, defeito reproduzido, risco de implementação e limite de evidência são identificados separadamente.

### P0 — limite de segurança para operação energética

**A01 — Ignição sem implementação e sem validação física de segurança.**  
Evidência: G/`main.py:415–418,540–548`; `confirmacao_motor.py:62–78`; F:1100–1141 e despacho F:1023–1056 sem comando de motor. Natureza: lacuna/gate, não acionamento indevido reproduzido. O botão permite a confirmação demonstrativa sem provar estado físico da bancada. A ausência de comando não garante o nível elétrico de um circuito externo conectado a pino não auditado. **Risco:** transformar a confirmação visual em autorização física durante a integração. **Mínimo:** manter o acionamento excluído do primeiro ciclo e identificar o botão como demonstração; qualquer futura liberação exige análise independente do circuito e intertravamentos locais, inclusive boot/reset/falhas. **Teste:** integração somente com ignição fisicamente desabilitada; comprovar que nenhum caminho de GUI/USB nessa etapa aciona carga energética. Não foi executado teste físico.

### P1 — integração e decisões bloqueantes

**A02 — Não existe transporte GUI↔ESP.**  
Evidência: G/`main.py:312–325,456–462,495–524,550–578`; imports de todos os módulos. Natureza: ausência declarada da Etapa 06. **Risco:** botão altera cronômetro/estado sem iniciar aquisição, e indicador é somente apresentação. **Mínimo:** criar camada serial e de protocolo separada dos widgets, sessão/IDs/reconexão, fila para thread Tk; fazer o estado exibido depender das respostas/eventos reais. **Teste:** descobrir sessão, consultar STATUS, configurar/tarar/iniciar/parar com dispositivo simulado e depois placa sem ignição; rejeições não podem mostrar “adquirindo”.

**A03 — Flash externa planejada não é o backend implementado.**  
Evidência: `Armazenamento.h:2,127`; F:347,591,1110,1173; `ComandosArmazenamento.h:75`. **Risco:** supor capacidade, CS, persistência ou validação de um componente que o firmware não usa. **Mínimo:** coordenação decidir formalmente entre manter LittleFS interno nesta entrega ou implementar a memória externa em etapa separada, após identificar chip/conexões. Não migrar silenciosamente nem trocar o backend durante integração USB. **Teste:** ESPACO/STATUS devem identificar o backend real; teste físico de gravação/reboot/capacidade nele. Código presente não comprova utilização da CJMCU-2516.

**A04 — Compilação para a placa e compatibilidade com o circuito não comprovadas.**  
Evidência: F:19–26,1110–1129; layout/asserts em `Armazenamento.h:38–41`; dependências sem versões completas travadas; `VALIDACAO.md`. Natureza: gate de evidência. **Risco:** incompatibilidade de API/core/partição, pinagem diferente da montagem ou uso de configurações antigas. **Mínimo:** registrar placa/FQBN, core, bibliotecas, partições e revisão do circuito; compilar sem mudar os pinos para “fazer funcionar”. **Teste:** build ESP32 e teste benigno de USB, HX711, LEDs e filesystem com a pinagem conferida. Aqui só build nativo concluído.

**A05 — Dados da GUI não têm adaptador para o contrato do ESP.**  
Evidência: G/`dados_memoria.py:13–39`, `main.py:328–367,491–493`, `perfis_calibracao.py:114–145`; F:787–850. **Risco:** nomes/observações excederem bytes, data rejeitada, milissegundos tratados como segundos, duplicação/perda de amostras e aplicação dupla do fator. **Mínimo:** validar bytes UTF-8/envelope completo; usar data ISO, converter ms→s apenas na apresentação; chave de amostra `(ensaio_id, indice)`; não reaplicar tara/fator ao empuxo já calibrado. O perfil novo gera UUID hex de 32 caracteres, mas a identificação livre da célula não tem limite equivalente. **Teste:** acentos, limites, nomes longos, dados fora de ordem/duplicados/lacunas, data e coeficiente negativo. Probe desta auditoria: célula com 33 “ç” é aceita localmente (66 bytes), contra máximo 32 do ESP.

**A06 — Receptor BT/1 e ponte para PC ausentes.**  
Evidência: `TransferenciaLoRa.h:109–159,163–224`; `PROTOCOLO_LORA.md`; nenhum firmware receptor nos ZIPs. **Risco:** receptor anterior tratar binário como JSON ou interpretar TX local como recebimento completo. **Mínimo:** manter o transmissor e implementar receptor por contrato em entrega própria; incluir definição da saída para Python. **Teste:** arquivo remontado byte a byte, ACK perdido, repetição de FIM/CONFIRMAR, cancelamento e reset de ambas as estações; bancada sem carga energética. Não bloqueia o primeiro ciclo USB.

**A07 — Histórico, salvamento e exclusão da GUI ainda são demonstrativos.**  
Evidência: G/`main.py:427–436`; `memoria.py:131–171`; `dados_memoria.py:42–69`. **Risco:** operador confundir exclusão da lista com exclusão no ESP, ou acreditar que o teste foi salvo/exportado no PC. **Mínimo:** integrar consulta/leitura e arquivo local com estado “cópia completa/incompleta”; tornar explícito o alvo de exclusão; implementar exportação depois da recuperação íntegra. **Teste:** adquirir, encerrar, recuperar tudo, reabrir no PC e comparar índices/tempos/kgf; exclusão parcial/protegida deve manter os itens corretos. CSV/Excel não existem hoje.

### P2 — confiabilidade

**A08 — Leitor local de perfis aceita coeficiente JSON duplicado. Defeito reproduzido.**  
Evidência: G/`perfis_calibracao.py:163–178` usa `json.loads` sem os hooks presentes na importação em 225–227; funções de validação nas linhas 57–67. Entrada com `"coeficiente":100,"coeficiente":200` foi carregada sem erro e resultou em **200.0**; a importação do mesmo arquivo recusou a duplicação. **Risco:** arquivo ambíguo/editado externamente alterar silenciosamente a calibração selecionada. Não se observou geração desse JSON pelo próprio aplicativo. **Mínimo:** unificar o parser estrito do cadastro e da importação, manter preservação/bloqueio diante de erro. **Teste:** ambos os caminhos rejeitam o mesmo arquivo e preservam bytes/perfis anteriores. Evidência: `gui_resultados.txt`, probes ao final.

**A09 — Latência de filesystem pode comprometer a cadência; não há diagnóstico suficiente de lacunas na aquisição física.**  
Evidência: F:1161–1202; `ComandosArmazenamento.h:205–219`; APIs síncronas usadas no loop. O índice aumenta quando uma leitura é gravada; não é um contador das conversões físicas produzidas pelo ADC. **Risco:** sequência contínua dar falsa impressão de amostragem regular, embora o tempo entre leituras varie por escrita/flush/catálogo. **Mínimo:** primeiro medir intervalos e latência máxima com gravação, telemetria e catálogo; estabelecer taxa/limites e só então decidir buffer/agendamento/telemetria. Não introduzir uma arquitetura complexa sem medição. **Teste:** carga conhecida e sinal de teste benigno, timestamps, maior intervalo, distribuição e contagem; repetir em ocupação crescente da memória. Testes nativos não modelam esse custo.

**A10 — Durabilidade física não equivale ao sucesso de write/flush/releitura.**  
Evidência: F:624–635,525–540,1202; `Armazenamento.h:89–159`. Existe flush periódico e verificação estrutural no fechamento, mas sem garantia de preservação dos últimos dados em corte de energia. **Risco:** contabilizar como persistidas todas as amostras emitidas antes de uma interrupção abrupta. **Mínimo:** explicitar a janela de perda como não caracterizada e conservar estados incompletos; executar ensaio físico antes de prometer durabilidade. **Teste:** reinícios/cortes controlados em ensaios descartáveis, sem ignição, comparar os registros anteriores e o último recuperável. O reboot do mock verifica lógica, não Flash elétrica.

**A11 — Congestionamento USB afeta atendimento de comandos e progresso de catálogo.**  
Evidência: F:140–145,1075; `ComandosArmazenamento.h:190`; testes `controle_congestionado` e `ar_catalogo_backpressure`. Há reserva de espaço para controles; quando insuficiente, RX e catálogo esperam. Aquisição não depende de ACK USB, mas FINALIZAR também passa por esse RX. **Risco:** aplicação que não drena a serial acumular respostas e atrasar comandos; exclusão total pode manter CATALOGO_OCUPADO enquanto aguardando saída. Não foi demonstrado travamento permanente do hardware. **Mínimo:** cliente ler continuamente em worker, limitar pedidos simultâneos, mostrar timeout/estado desconhecido e reconciliar; não tratar FINALIZAR por USB como parada física de emergência. **Teste:** pausar leitura, gerar respostas, retomar e conferir resposta/estado sem reexecutar pedidos destrutivos. Se houver requisito de parada independente do canal, tratá-lo como requisito separado.

### P3 — documentação e manutenção

**A12 — Roteiro contradiz ENVIAR obrigatório por ID.**  
Evidência: `COMANDOS_E_TESTES.md:102` versus F:713–716, `PROTOCOLO_USB.md` e teste `lr_ids_obrigatorios`. **Risco:** teste manual/integrador enviar pedido incompatível. **Mínimo:** remover a afirmação de seleção implícita; manter `ensaio_id` obrigatório. **Teste:** conferir exemplos contra o parser e executar o cenário já existente. Não alterar firmware para cumprir o texto obsoleto.

**A13 — Limites de manutenção e testes da GUI.**  
Evidência: F:116–125 retém oito respostas/eventos; os três `.h` definem funções/estado e dependem da ordem de inclusão; G/`main.py` concentra 840 linhas; GUI não traz suíte automatizada. O log acumula lista sem limite e a copia no polling (`log_erros.py:44–74`; `main.py:172–179`). **Risco:** manutenção dificultada e consumo crescente em sessões com muitos erros, especialmente após integrar rádio/USB. **Mínimo:** adicionar testes de contrato na integração; limitar/rotacionar apresentação do log conforme uso real; modularização incremental descrita abaixo. **Teste:** regressões existentes e sessão prolongada com erros repetitivos; medir RAM/latência, sem supor que o firmware esteja hoje “sobrecarregado”.

## 7. Revisão por subsistema e pontos a preservar

### Aquisição e calibração

A fórmula implementada é `(leitura bruta − tara interna) / coeficiente`, com coeficiente em **contagens/kgf**. `get_value()` já considera o offset aplicado por `set_offset`; o sketch não faz uma segunda subtração. Tara usa dez leituras brutas, soma int64 e referência int32. Configuração invalida tara; aquisição bloqueia fator não configurado e sensor/tara indisponíveis. Nenhuma dessas responsabilidades deve migrar para Python, pois o registro precisa continuar autônomo.

O primeiro tempo é zero quando chega a primeira amostra, não quando o usuário clica. A duração final decorre do estado do ESP. Não há parâmetro de duração programada, parada por cronômetro no ESP ou medição de taxa efetiva neste contrato. A GUI atual mede somente o tempo local de demonstração. Não há filtragem digital adicional implementada nesta versão; a média das dez leituras é da **tara**, não média móvel das amostras de empuxo.

### Armazenamento

Formato 3: cabeçalho 392 bytes, amostra 64 bytes, rodapé 56 bytes; total de um registro fechado = `448 + 64 × quantidade`. Reserva conservadora de 8192 bytes, além do necessário para registros. Capacidade útil depende da partição real, não dos 16 Mbits do módulo externo planejado.

Preservar: IDs sessão+contador, verificação de colisão antes de FILE_WRITE, não formatação automática (`LittleFS.begin(false)`), metadados congelados por ensaio, CRC e identificação por amostra, catálogo derivado do diretório, reconhecimento de incompletos e proteção de arquivos em uso. `concluido` indica estrutura/rodapé válidos, não varredura prévia de todas as amostras; leitura por bloco detecta corrupção individual. Não reclassificar blocos corrompidos como zeros e não excluir originais após transmissão.

A gravação repete ID e coeficiente em cada amostra. Isso aumenta tamanho, mas também participa da validação de identidade. Alterar essa redundância mudaria formato e exigiria migração; não há justificativa para fazê-lo neste ciclo.

### LoRa e LEDs

A máquina BT/1 valida registros, calcula hash incremental, envia blocos de 128 bytes em quadros de até 218 bytes, espera ACK e permite reparos. Limites: 2 s por espera após TX_DONE, quatro emissões por ciclo de quadro, 64 reagendamentos globais, 2 h totais. Timeout de TX_DONE de 5 s desabilita rádio sem interromper a aquisição ao vivo por esse motivo.

Preservar a distinção entre `envio_local_concluido` e `recebimento_confirmado`; ACK de FIM não substitui CONFIRMAR. O CRC identifica corrupção, não prova sozinho persistência no receptor: CONFIRMAR é uma declaração do receptor, que precisa implementar validação e salvamento antes de responder. Cancelamento é local; o receptor precisa expirar a sessão parcial.

Telemetria ao vivo usa a última leitura disponível e oportunidades a cada 250 ms; **não é espelho integral do arquivo**. Gravação e envio BT/1 são mutuamente exclusivos, mas aquisição e telemetria podem coexistir. O amarelo acompanha TX local e apaga na espera; o verde só acende após primeira amostra gravada. Pequenas latências dos LEDs dependem do loop real; não interpretar LED como ACK remoto.

### GUI e qualidade

Preservar o layout, temas, calendário, unidade kgf, separação de perfis e ensaios, conversão explícita do legado em N, backup antes da primeira gravação convertida, gravação atômica de perfis, rejeição de importação em lote inválida, proteção do perfil carregado e logs com lock. Os testes sequenciais de conflito de arquivo passaram; isso não demonstra exclusão mútua entre processos simultâneos.

Não é necessário instalar Matplotlib/pandas apenas por terem sido citados no planejamento. Canvas já desenha o histórico. A escolha do gráfico ao vivo deve considerar o volume real. Não remover cache de comandos, CRC, timeouts ou estados para reduzir número de linhas. Cálculos futuros de média/impulso/exportação podem permanecer no PC.

## 8. Modularizar o .ino?

**Sim, pode ajudar a manutenção, mas não é pré-requisito para a primeira integração USB nem prova de melhoria de desempenho.** O sketch possui 1.226 linhas; armazenamento, catálogo e BT/1 já estão separados fisicamente. Contudo, os `.h` contêm definições de funções e variáveis globais, não interfaces independentes. Copiá-los para vários `.cpp` sem refatorar o estado pode produzir dependências circulares/definições múltiplas.

Proposta posterior: separar configuração/pinagem, aquisição/calibração, protocolo USB, armazenamento, catálogo e transferência; tornar explícitas as dependências e manter `setup/loop` como coordenação. Fazer uma extração por commit, mantendo bytes do formato, JSON, semântica de IDs, pinos e rádio. A tabela `Opcoes` é uma indireção pequena, não um problema de sobrecarga demonstrado. Checagens redundantes de fator/tara em `acaoIniciar` são candidatas a limpeza posterior, sem prioridade sobre integração.

Critério de aceitação de cada extração: 141 testes atuais + novos testes relevantes, build ESP32 e comparação de mensagens/arquivos de referência. Só medir ganho de memória/tempo com build e instrumentação reais; contagem de linhas não estima sobrecarga do ESP32.

## 9. Confronto específico com checkpoint e documentação

### 9.1 Checkpoint de 15/11/2026

| Item do checkpoint | Correção após inspeção |
|---|---|
| Firmware entregue V0.17 | Confirmado por VERSAO, constante e teste IDENTIFICAR |
| 141/141 testes relatados | Agora reproduzidos nesta auditoria, ainda em ambiente nativo simulado |
| Flash SPI 16 Mbits como registro local estabelecido | Planejamento não implementado; backend atual é LittleFS interno |
| Interface Python Tkinter + Matplotlib | Tkinter confirmado; gráficos atuais em Canvas, sem Matplotlib |
| Perfis e kgf | Implementados localmente; não aplicados ao ESP pela GUI |
| Interface e controles definidos | Visual existe, mas iniciar/parar não comandam bancada; salvar/tara/LoRa são prévias |
| Recuperação completa LoRa | Transmissor implementado/testado; receptor/ponte PC e ACK físico ausentes |
| Pinos históricos não devem ser presumidos | Mantido; tabela real é 33/32, 21/14/26, 4/2 e SPI 18/19/23 |
| LED verde/amarelo | Lógica confirmada no código; LEDs físicos não testados |
| Independência do PC | Aquisição/arquivo e BT/1 não dependem de ACK USB; catálogo/atendimento de comandos têm backpressure |
| Iniciar motor com segurança | Apenas demonstração visual; não há função de ignição no firmware |
| Software anterior com filtros/watchdog | Não encontrados como funções implementadas nesta V0.17; timeouts não são equivalentes |
| Material V0.16 incluído | Documentação/testes herdados; não há snapshot integral V0.16 nem histórico para diff |
| Git mantido pelo autor | Relato preservado; commit/tag/repositório não disponíveis nesta auditoria |

A classificação P0/P1/P2/P3 deste relatório é a de **achados** solicitada no prompt; não confundir com P0–P4 usados no checkpoint para ordenar fases de trabalho.

### 9.2 Documentos TG2 efetivamente consultados

| Documento/seção | Texto ou planejamento encontrado | Atualização necessária, sem extrapolar evidência |
|---|---|---|
| `TG2_Texto_Base.docx` v23, resumo, §4.3, §5.1, cronograma | Firmware/software ainda não iniciados | Informar V0.17 e Etapa 06 existentes; distinguir testes nativos de integração física pendente |
| TG2 v23, §4.3 | Arduino substitui ESP-IDF; aquisição/Flash/LoRa ainda planejados | Arduino já corresponde ao código; descrever funções implementadas e dependências verificadas |
| TG2 v23, §4.4 | Flash substitui SD; extração ainda a definir | Especificar LittleFS interno efetivo, formato 3 e leitura USB por blocos; externa como pendência/decisão |
| TG2 v23, §4.4 | Tempo real como objetivo e envio posterior como alternativa | Código tem ambos, com propósitos distintos: ao vivo com perdas possíveis; BT/1 para arquivo integral após teste |
| TG2 v23, §4.4 e Quadro 2 | Controle pelo programa, gráfico, média e Excel | Ainda não integrados na GUI; média/Excel permanecem futuros |
| TG2 v23, §3.4/§4.3 | Modelo de calibração com coeficientes/offset; filtros/watchdog propostos | Relacionar modelo à tara interna e divisão por coeficiente contagens/kgf; não afirmar filtro/watchdog de aplicação existente |
| TG2 v23, §5.2 e tabela de verificação | Resultados propostos em N | Interface operacional é kgf; quando apresentar N na análise científica, indicar conversão, sem criar seletor de unidade na GUI |
| `TCC-v0.03_valdir20251119.docx`, §2.3 | ESP-IDF, TCP/IP, Socket, Matplotlib, CSV/pandas, média móvel e rejeição de picos | Marcar como arquitetura anterior; não copiar para descrição da implementação atual |
| TCC antigo, §2.4 | Coeficiente descrito como N por unidade digital; calibração automática prevista | Convenção do firmware é a inversa em contagens/kgf; cadastro manual não realiza calibração automática |
| TCC antigo, §2.4; TG2 v23, §3.5 | Metas de 2% e 100 ms | Não são resultados comprovados pelos testes de software desta auditoria |

O TG2 atual já trata várias afirmações antigas com cautela e já registra Arduino/LoRa. Não seria correto dizer que o documento atual inteiro ainda adota ESP-IDF/TCP-IP. Comparação de software não atualiza automaticamente a situação das placas, soldagem, célula ou alimentação.

## 10. Testes executados, ambiente e limitações

| Verificação | Ambiente/método | Resultado |
|---|---|---|
| Manifesto firmware | `sha256sum -c SHA256SUMS.txt` | Aprovado |
| Preservação das entradas | Comparação byte a byte dos 180 arquivos extraídos com ZIPs | Aprovado; códigos originais intactos |
| Build nativo | g++ Debian 14.2.0, C++17, warnings como erro, ASan+UBSan, ArduinoJson 7.4.2 | Aprovado |
| Suíte firmware | `run_tests.py` na cópia de execução | 141/141; logs novos anexos |
| Python | Python 3.12.14, Linux x86_64; módulos originais importados sem bytecode; dados de teste temporários | 24/24 verificações aprovadas |
| Chave duplicada do cadastro | Probe independente, arquivo temporário | Defeito A08 reproduzido |
| Limite de texto do perfil | Probe de célula com 66 bytes UTF-8 | Divergência A05 demonstrada |
| Tentativa de janela Tk | Tk 9.0; `tkinter.Tk()` | Não abriu: `no display name and no $DISPLAY environment variable` |
| Disponibilidade ESP32 | Consulta `arduino-cli core list` | Nenhuma plataforma instalada; inicialização também reportou índices ausentes/rede indisponível |

Build/execução nativa usados (raiz de trabalho da auditoria):

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -no-pie \
  -Iworkrun/fw/validacao_nativa/mocks \
  -I/workspace/scratch/bd3e70e33512/test_dependencies/ArduinoJson-7.4.2/src \
  workrun/fw/validacao_nativa/test_firmware.cpp \
  -o workrun/fw/validacao_nativa/test_firmware
python3 workrun/fw/validacao_nativa/run_tests.py
python3 deliverables/evidencias/test_auditoria_gui.py audit/gui/Interface_Bancada_Etapa_06
```

O ArduinoJson foi encontrado como dependência local já disponível, versão conferida no próprio cabeçalho. Não integra o ZIP recebido. Ao reproduzir fora deste ambiente, substituir seu caminho por instalação da **mesma versão**. O runner configura `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` e `UBSAN_OPTIONS=halt_on_error=1`; portanto, **LeakSanitizer não foi executado**. O relógio e falhas do mock são determinísticos e não reproduzem timing do ADC/SPI/RF ou desgaste/cache da Flash.

Não executados: build com toolchain ESP32; upload; Windows/`INICIAR.bat`; avaliação visual dos widgets/temas/resoluções; COM/driver/DTR/RTS; consumo de RAM/stack no ESP; testes elétricos, calibração metrológica, recuperação após corte real e recepção BT/1 física. Ausência de display impediu teste visual; ausência de plataforma/hardware/receptor impediu os demais. Os relatos visuais do README da Etapa 06 são evidência anterior, não reexecução desta auditoria.

## 11. Próximas etapas pequenas e verificáveis

Esta é uma proposta para decisão no chat de coordenação; nenhuma etapa abaixo foi implementada.

| Etapa | Escopo e arquivos afetados futuramente | Critério de conclusão |
|---|---|---|
| 1 — Fixar baseline e decisões | Manifesto/registro de build; documentos de arquitetura/pinagem; decidir backend | Tag/commit identificado, Flash interna/externa decidida, placa/core/libs/partições anotados; ignição fora do ciclo USB |
| 2 — Corrigir inconsistências localizadas | `COMANDOS_E_TESTES.md`; `perfis_calibracao.py` e testes | ENVIAR sempre documentado com ID; duplicatas rejeitadas também no cadastro; backups preservados |
| 3 — Compilar e validar base na placa | Configuração de build; os quatro fontes só se houver falha demonstrada | Build ESP32; IDENTIFICAR/STATUS, sensor, tara, arquivo e LEDs verificados sem ignição |
| 4 — Conectar e identificar pela GUI | Novos módulos `transporte_serial.py`/`protocolo.py` (nomes propostos), `main.py`, dependências | COM→IDENTIFICAR→STATUS; sessão/IDs corretos; leitura em worker; desconexão/reconexão sem congelar UI |
| 5 — Calibração/tara reais | Adaptador, `calibracao.py`, `main.py` | Configurar→confirmar→tarar→aguardar terminal; não permitir início com estado desconhecido; zero/negativos/timeout testados |
| 6 — Aquisição USB e gráfico | `main.py`, modelo de ensaio e gráfico; adaptador | Aceito não significa iniciado; tempo/índice/kgf vêm do ESP; FINALIZAR e terminal reconciliados; lacunas identificadas |
| 7 — Recuperar e salvar ensaio completo | `memoria.py`, modelo/repositório de ensaios, adaptador | Catálogo real, paginação, blocos, CRC inválido visível; arquivo PC reaberto idêntico em amostras/metadados |
| 8 — Exclusões e exportações | `memoria.py`, `dialogos_memoria.py`, módulos de exportação | Alvo identificado, resultados parciais tratados; CSV/Excel com unidades corretas; originais mantidos após envio/exportação |
| 9 — Caracterizar aquisição/persistência | Instrumentação pontual do firmware se necessária; plano/logs físicos | Taxa, maior intervalo, janela de perda e recuperação caracterizados no backend escolhido; não presumir aprovação |
| 10 — Receptor BT/1 e GUI LoRa | Projeto próprio do receptor; adaptador/status na GUI; preservar contrato TX | Arquivo igual ponta a ponta, perda/duplicação/reparo/reset testados, CONFIRMAR só após salvar |
| 11 — Documentar e modularizar se útil | TG2/README; extrações pequenas do .ino após integração estável | Documentos separados em implementado/testado/pendente; uma extração por vez com regressões/build |

Se a Flash **externa** for requisito obrigatório da entrega imediata, sua implementação deve virar etapa própria após a decisão 1, antes da validação definitiva de persistência. Ela não deve ser misturada com adaptação da interface, receptor e modularização em uma mesma mudança.

**Decisão técnica recomendada para a próxima rodada:** preservar a V0.17, resolver a definição do backend e a compilação da placa, corrigir as duas inconsistências pontuais, depois começar pela conexão USB e leitura de STATUS. Isso fornece uma primeira integração verificável sem reescrever a bancada ou adicionar ignição.
