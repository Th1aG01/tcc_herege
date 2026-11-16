# CHECKPOINT 02 — TCC Bancada de Empuxo

**Data de referência:** 15/11/2026 (America/Sao_Paulo)  
**Marco:** retorno da auditoria da V0.17 do firmware e da Etapa 06 da interface  
**Fonte principal:** `AUDITORIA_TCC_V017_INTERFACE06(1).md` (diagnóstico somente leitura).  
**Natureza:** consolidação de coordenação, **não** é nova auditoria independente dos arquivos de código.

## 1. Resultado executivo

- **Não refazer o firmware do zero.** A auditoria encontrou uma base coerente e testável; não reproduziu falha P0 de execução no ambiente nativo simulado.
- Firmware: **1.908 linhas** em quatro fontes principais; `Bancada_Empuxo_V0_17.ino` tem **1.226 linhas** e os outros módulos tratam armazenamento, catálogo e transferência LoRa.
- Interface: **2.599 linhas** nos módulos Python; é principalmente **demonstrativa e ainda não se comunica com o ESP32**.
- Testes **reexecutados pelo auditor**: 141/141 cenários nativos do firmware (com simuladores de hardware) e 24/24 verificações Python. Não houve build real para ESP32, teste físico, tela Tk ou LoRa ponta a ponta. Esses resultados não foram reproduzidos novamente na coordenação.
- Há um defeito reproduzido no cadastro de perfis Python: JSON local aceita chave `coeficiente` duplicada; importação externa rejeita.
- O firmware usa **LittleFS da Flash interna do ESP32**, não a **Flash SPI externa de 16 Mbits** prevista no projeto.
- A transferência BT/1 está presente **no transmissor**; não há firmware receptor e ponte receptor→PC no material auditado.
- **Ignição não está implementada.** Manter desconectada/desabilitada fisicamente durante todo o ciclo inicial de integração USB; nenhum botão de confirmação visual equivale a intertravamento seguro.

## 2. Atenção: situação de hardware versus firmware

**Pinagem encontrada no código V0.17 (não validada na PCB):**

| Subsistema | GPIO da V0.17 |
|---|---|
| HX711 DT / SCK | 33 / 32 |
| LoRa CS / RST / DIO0 | 21 / 14 / 26 |
| SPI SCK / MISO / MOSI | 18 / 19 / 23 |
| LED execução / LoRa | 4 / 2 |

LoRa: 915 MHz, SyncWord 145 decimal, potência 2 dBm, clock SPI 1 MHz, oportunidade de telemetria a cada 250 ms. Parâmetros RF SF/BW/CR/preâmbulo/CRC não estão fixados explicitamente pelo sketch. Não recuperar pinos de versões históricas sem comparar com o esquema elétrico atual.

**Divergência importante:** relatos históricos mencionavam CS do LoRa no GPIO 5, enquanto a V0.17 declara GPIO 21. A pinagem real da montagem/PCB precisa ser conferida antes de qualquer upload, e não alterada por suposição.

## 3. Fronteiras reais dos sistemas

**ESP32 implementado (em código, sujeito a prova física):** USB JSON 115200, sessão e IDs, HX711/tara/calibração kgf, arquivos LittleFS com CRC e catálogo, transmissão LoRa ao vivo parcial, envio BT/1 com ACK e reparo, LEDs.

**Python implementado:** temas/layout, calendário, perfis locais, logs, histórico de demonstração, diálogos. **Ausente:** transporte serial, sessões e comandos reais, aquisição e gráfico ao vivo, recuperação/salvamento de ensaios reais, exportações CSV/Excel de ensaios, controle motor físico.

**Receptor:** contrato BT/1 definido e mock testado; código instalável/ponte PC e confirmação física ausentes.

## 4. Decisões de coordenação

1. **Preservar a baseline V0.17 e Interface Etapa 06**, com commit/tag identificável no Git antes das alterações.
2. **Não reescrever ou modularizar por contagem de linhas.** Refatoração estrutural somente quando um problema concreto exigir, com regressão.
3. **Tratar Flash externa como decisão de arquitetura ainda aberta.** Para uma demonstração USB inicial, LittleFS interno pode servir como backend provisório, mas o projeto não poderá ser documentado como usando a Flash externa sem implementação/teste. Se Flash externa for requisito obrigatório da entrega, programar etapa própria e testes físicos.
4. **Prioridade imediata: conferir PCB/pinos, sistema de arquivos e configuração de placa; fazer compilação real e teste benigno na bancada antes de conectar GUI.**
5. Corrigir pontualmente a validação de JSON duplicado e o roteiro `COMANDOS_E_TESTES.md` que sugere `ENVIAR` sem ID. **Não** alterar protocolo para se adequar a documento obsoleto.
6. **Primeira integração:** GUI USB `IDENTIFICAR` → `STATUS`, sem ignição, com resposta real, IDs/sessão e worker que não bloqueie Tk.
7. Evoluir em etapas: calibração/tara; início/fim e gráfico; recuperação e salvamento; exportações; receptor BT/1; documentação final.
8. LoRa ao vivo não é cópia integral do ensaio, e `ENVIO_LOCAL_CONCLUIDO` não é `ENVIO_CONFIRMADO`.

## 5. Próximo ciclo (pequeno e verificável)

### Gate A — Congelar e validar a base embarcada

**Entradas:** ZIP V0.17, esquemático/PCB real, identificação da placa, Arduino core/bibliotecas, partição LittleFS, decisão sobre Flash externa.  
**Saídas:** commit/tag, manifesto das versões, tabela de pinos conferida, log de build ESP32, teste sem ignição de USB/IDENTIFICAR/STATUS, HX711/tara e armazenamento.  
**Não fazer:** alterar simultaneamente armazenamento, rádio, GUI e ignição.

### Gate B — Correções pontuais

Consertar leitura de perfis JSON locais para rejeitar chaves duplicadas exatamente como a importação, preservando os dados e backups; corrigir instrução de `ENVIAR` no roteiro manual; criar testes regressivos correspondentes.

### Gate C — Primeiro elo da interface real

Criar adaptador serial isolado dos widgets Tk, implementando identificação, estado, sessão/ID, leitura assíncrona e reconexão. Aprovação: indicador só declara conexão após resposta válida do ESP; GUI continua responsiva; nenhum teste inicia pelo clique sem confirmação de estado real.

## 6. O que está *pendente*, e não deve ser anunciado como validado

- Compilação e upload para a placa ESP32 real, pinagem física, taxa real do HX711, estabilidade/calibração metrológica.
- Flash externa 16 Mbits, comportamento de persistência em corte de energia, tempo de bloqueio de operações LittleFS.
- Ensaio real controlado via GUI e preservação integral de amostras.
- Receptor LoRa, ACK físico/CONFIRMAR real e arquivo reconstruído byte a byte no PC.
- Ignição e intertravamentos: fora do primeiro ciclo; validação específica de segurança obrigatória antes de operação energética.

## 7. Uso de chats e modos

- **Coordenação — este chat:** decisões, checkpoints, prioridades, avaliação de resultados.
- **Chat firmware/Chat GUI:** alterações localizadas, entendimento, testes pontuais, explicações.
- **Work:** tarefas de muitos arquivos e verificação estruturada (auditoria já executada; integração extensa quando a base estiver estável).
- **Novo chat somente quando mudar a frente principal ou o histórico atrapalhar.** Uma entrega exige arquivos atuais e este checkpoint; não confiar em lembranças sobre código anterior.
- A documentação TG2 deve distinguir planejado, implementado, testado em simulação e validado em hardware.

## 8. Observações de documentação

Documentos antigos citam ESP-IDF/TCP-IP, Matplotlib, filtros e calibração automática. São planejamento histórico, não retrato do firmware/GUI atual. O TG2 recente já menciona Arduino em parte do texto; revisar seção por seção para não atribuir funções ainda ausentes.

**Próxima decisão pendente:** Flash interna provisória versus Flash externa obrigatória já nesta entrega, com base na PCB física e no escopo formal.
