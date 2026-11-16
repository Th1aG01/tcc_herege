# CHECKPOINT 01 — Bancada de Empuxo / TG2

**Data:** 15/11/2026  
**Marco informado pelo autor:** firmware `Bancada_Empuxo` **V0.17** concluído no chat de desenvolvimento.  
**Natureza deste documento:** consolidação de decisões e evidências relatadas, **não** auditoria técnica da V0.17.  
**Projeto:** sistema eletrônico e computacional de aquisição, armazenamento, transmissão e visualização de empuxo da bancada para a equipe BRAVO/Univap.

## 1. O que está estabelecido

- Prioridade de curto prazo: concluir o software necessário e atualizar a documentação do TG2 com fatos verificáveis; evitar aumento de escopo.
- Ferramentas principais: ESP32 DevKit V1 programado em C++/Arduino IDE 2; computador Windows com aplicativo Python, interface Tkinter e gráficos Matplotlib; Git com histórico de versões mantido pelo autor.
- Medição: célula definitiva M&C HZ tipo S, 2000 kgf; HX711 como conversor de aquisição; células menores para testes em bancada. Unidade operacional padrão **kgf** na interface. A aquisição deve gerar sequência de amostras e tempos de medição coerentes.
- Registros: memória Flash SPI anunciada como 16 Mbits (CJM/CJMCU-2516); preservar os dados originais após envio, sem apagamento automático; exclusão somente mediante operação explícita com proteções.
- Comunicação: USB/Serial e LoRa com funções distintas. Na evolução recente do firmware, protocolo serial JSON a 115200 baud foi relatado; a compatibilidade real com a interface ainda precisa ser conferida. LoRa da bancada historicamente configurado em 915 MHz; **não inferir pinagem do histórico**, conferir nos arquivos da V0.17 e no circuito.
- Indicadores: LED verde durante ensaio, LED amarelo conforme estado efetivo de transmissão LoRa.
- Independência: a perda da conexão com o PC ou a indisponibilidade do receptor não deve, sozinha, interromper ou apagar registros locais.
- Segurança: o sistema de ignição só poderá ser habilitado com intertravamentos locais e validação independente; durante testes de software, utilizar hardware de ignição fisicamente desabilitado.

## 2. Situação das entregas

| Item | Estado conhecido em 15/11/2026 | Grau de evidência |
|---|---|---|
| Firmware `Bancada_Empuxo` V0.17 | Entregue pelo chat dedicado | Informado pelo autor / histórico da entrega |
| Transferência de ensaio por LoRa | V0.17 **relatada** com `ENVIAR`, `CONSULTAR_ENVIO`, `CANCELAR`, identificação de blocos, CRC, confirmação/retransmissões limitadas e distinção entre envio local e confirmação do receptor | Relato de implementação; não examinado neste checkpoint |
| Testes do firmware V0.17 | Chat que produziu a V0.17 relatou **141/141 testes nativos aprovados** e aprovação das APIs públicas LoRa/HX711 | Resultado relatado; conferir relatório e execução reproduzível |
| Compilação Arduino, upload e teste físico da V0.17 | **Não comprovados** | Pendente |
| Receptor compatível com o novo protocolo LoRa | Não alterado na etapa V0.17 | Pendente |
| Envio LoRa ponta a ponta com ACK real | **Não validado** | Pendente |
| Leitura HX711 sob carga | Realizada em etapas anteriores com célula de testes | Evidência anterior, não valida V0.17 |
| Flash instalada e testada fisicamente | Não comprovado no histórico recuperado | Pendente |
| Interface Python | Refatorada em chat próprio; aparência, controles e fluxos definidos | Implementação relatada; arquivos atuais não auditados |
| ESP32 ↔ Python | Integração da versão atual ainda pendente | Pendente |
| Documentação TG2 | Rascunhos anteriores existem, mas alguns pontos estão desatualizados | Revisão obrigatória |

**Artefato mencionado na entrega V0.17:** `Bancada_Empuxo_V0_17_Transferencia_LoRa.zip`; segundo o histórico, contém firmware `.ino`, três cabeçalhos `.h`, documentação `TESTES_LORA.md` e material da V0.16. Este arquivo ZIP **não estava disponível para abertura técnica neste checkpoint**; verificar conteúdo real quando fornecido no ambiente de auditoria.

## 3. Arquitetura-alvo e fronteiras de responsabilidade

### ESP32 — manter local e independente

- Aquisição e identificação temporal das amostras do HX711.
- Tara e aplicação da calibração, em unidade operacional kgf.
- Estados da sessão, início/parada e tratamento de anomalias críticos.
- Gravação e recuperação dos dados originais na Flash.
- Comunicação USB/Serial e LoRa, incluindo confirmação, tolerância a perdas e limites de retransmissão se o contrato exigir.
- LEDs e proteções locais indispensáveis, sem depender de PC ou enlace de rádio.

### Python no computador — aplicação e análise

- Interface gráfica escura, com alternância claro/escuro e redimensionamento.
- Controle e apresentação de estado, conexão, logs e mensagens de erro.
- Gráficos em tempo real, abertura de registros, nomes e observações dos ensaios.
- Análise e métricas derivadas (empuxo máximo/médio, impulso, quando dados e qualidade permitirem).
- Exportação CSV/Excel, histórico, relatórios e apresentação da calibração.

### Receptor LoRa — componente separado

- Deve implementar o contrato da V0.17 para confirmação de blocos, solicitação de retransmissão e entrega confiável ao computador.
- Sem atualizar e testar o receptor, **não** afirmar recuperação remota ponta a ponta.

## 4. Funcionalidades esperadas da interface (necessitam cotejo com o código atual)

- Identificação do teste: nome, data, observações.
- Controles: iniciar/parar teste; tara; calibração; salvar/abrir; limpeza de gráfico; cronômetro; transmitir teste; apagar registros com confirmação; logs de erros; diagnóstico de conexões.
- Controle de ignição em destaque, sujeito a confirmações e principalmente intertravamentos físicos e locais próprios; não basta diálogo de confirmação na GUI.
- Sem campo de offset exposto ao operador; todas as medidas de força da GUI apresentadas em kgf.

## 5. Metodologia aprovada para o restante do projeto

1. **Chat de coordenação (este):** decisões, riscos, prioridades, checkpoints, prompts e relatórios.
2. **Chat de firmware:** correções pequenas e implementação localizada do ESP32 a partir de versão exata.
3. **Chat de interface:** manutenção localizada do aplicativo Python.
4. **Chat de integração:** contrato de comunicação, testes cruzados e resultados.
5. **Chat de documentação:** TG2, referências, figuras, resultados comprovados e estudo para defesa.

O Git e os arquivos de versão são fontes de verdade. Abrir novo chat por mudança de responsabilidade ou deterioração do contexto, **não** por um número fixo de mensagens. Rever o andamento a cada 3–5 alterações relevantes.

**Uso de modos:** Chat como padrão para planejamento, explicações, ajustes localizados e acompanhamento; Work (ou ferramenta apropriada de desenvolvimento) para auditorias multilínguas/multiarquivo, integração extensa, análise cruzada e documentação de maior porte. Limites de uso não devem alterar os critérios de segurança e teste.

**Ciclo obrigatório:** escopo e critério de aceitação → revisão do código real → alteração mínima → testes reproduzíveis → evidências de bancada quando aplicáveis → Git → atualização do checkpoint.

## 6. Prioridade do próximo ciclo

- **P0:** auditoria somente-leitura da V0.17 e Python atual. Confirmar versão, estrutura, pinagem, dependências, comandos, protocolo, segurança, Flash e requisitos atendidos. Não implementar nada ainda.
- **P1:** corrigir erros bloqueantes encontrados e manter/atingir aquisição de ponta a ponta USB/Serial → GUI → registro, com ignição desabilitada nos testes.
- **P2:** verificar Flash real, integridade e persistência dos ensaios; validar leitura/listagem/recuperação.
- **P3:** compatibilizar receptor LoRa e validar ACK/CRC/retransmissão com hardware.
- **P4:** completar exportações, acabamento, documentação e funções de conveniência.

Segurança local, ausência de perda silenciosa de dados e integridade da aquisição não são funcionalidades opcionais de baixa prioridade.

## 7. Riscos e questões de auditoria

1. **Código V0.17 não revisado neste checkpoint.** Não concluir que está correto porque houve testes nativos; ainda falta compilação, upload e hardware real.
2. **Possível sobrecarga do ESP32:** examinar funções duplicadas e cálculos que possam migrar para Python; não reduzir código em detrimento de autonomia, registro e segurança.
3. **Compatibilidade do protocolo:** comparar nomes, tipos, esquema JSON, separador, tempo de resposta, identificadores, unidades, números de erro e estados com o front-end.
4. **LoRa:** verificar tamanho real dos pacotes, CRC, janelas de timeout e ACK, colisões de estados, não bloqueio da aquisição; diferenciar TX local e confirmação remota.
5. **Flash:** confirmar part number real, capacidade útil, limites de escrita e comportamento após queda de energia.
6. **HX711:** verificar taxa efetiva e perdas amostrais sob gravação e envio; medição de 24 bits não implica precisão metrológica de 24 bits.
7. **Pinos:** conferir esquema atual contra o código e conflitos no barramento SPI; não reutilizar cegamente pinagem antiga.
8. **Ignição:** auditoria especial de estado seguro na inicialização, falhas/desconexão e comandos não autorizados; testar inicialmente sem sistema pirotécnico.
9. **Documentos antigos têm divergências:** o `TCC-v0.03_valdir20251119.docx` ainda descreve ESP-IDF, TCP/IP e medições N/kgf, embora o projeto atual tenha adotado Arduino e LoRa e padrão kgf. O antigo `Plano_Software_TG2.md` foi elaborado em outubro e contém propostas iniciais já ultrapassadas, além de uma identificação de autoria divergente; revisar antes de reutilizá-lo no TCC.

## 8. Critérios de status

- **Planejado:** ainda não implementado ou não inspecionado.
- **Implementado (relatado):** o código foi entregue ou descrito, mas sem evidência independente nesta rodada.
- **Validado em software:** build e testes especificados reproduzíveis aprovados, com artefatos e versões identificados.
- **Validado em bancada:** teste real do hardware, com procedimento, condições, logs e resultado registrados.

Nenhuma prova simulada deve ser apresentada como prova física na documentação.

## 9. Entrada necessária para auditoria completa

- ZIP integral `Bancada_Empuxo_V0_17_Transferencia_LoRa.zip` **ou** pasta da V0.17 com seus `.ino`/`.h` e testes.
- Projeto completo da GUI Python atual (módulos e `requirements.txt`, se houver).
- Documentos de protocolo/comandos e resultados de testes da V0.17, sobretudo `TESTES_LORA.md`.
- Pinagem/esquema elétrico revisados, quando disponíveis.
- Referência do commit/tag da versão no Git (se existir).

## 10. Próxima decisão

**Realizar auditoria com Work, inicialmente sem modificar nenhum arquivo**, usando o prompt separado `PROMPT_AUDITORIA_WORK_TCC.md`. Após o relatório, voltar ao Chat de coordenação para priorizar modificações. Congelar a V0.17 como baseline auditável; não substituir nem apagar essa referência antes dos testes.
