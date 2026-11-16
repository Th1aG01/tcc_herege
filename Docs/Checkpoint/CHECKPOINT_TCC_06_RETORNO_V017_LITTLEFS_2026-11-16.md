# CHECKPOINT 06 — Retorno ao LittleFS interno e congelamento da investigação da Flash externa

**Projeto:** TCC — Bancada de Empuxo BRAVO / UNIVAP  
**Data da decisão:** 16/11/2026 (America/Sao_Paulo)  
**Natureza:** checkpoint de coordenação e transição de contexto; **não é entrega de firmware**, compilação nem validação física nova.  
**Estado:** decisão arquitetural aprovada; retorno operacional à V0.17 **ainda a verificar no hardware**.

## 1. Decisão vigente — única fonte para as próximas tarefas

1. **Referência de firmware ativa:** `Bancada_Empuxo_V0_17_Transferencia_LoRa(2).zip`, com **LittleFS interno do ESP32**. O conteúdo deste ZIP é byte a byte a versão de V0.17 usada na auditoria, de SHA-256 `8e3f904c3f3a25881d212b8fa7d5f427617c3544565aab7e0bd9b561b71b5f57`.
2. **V0.18 — Flash SPI externa:** passa a ser **ramo experimental arquivado**, não a referência para novos desenvolvimentos. Guardar integralmente seu ZIP e o histórico no Git; não incorporar modificações da V0.18 à V0.17 sem requisito e revisão explícitos.
3. **Armazenamento principal nesta fase:** LittleFS na Flash interna do ESP32. Preservar ensaios, arquivos e formato de dados existentes. **Não formatar nem apagar automaticamente.**
4. **Flash externa CJMCU-2516/W25Q16 de 16 Mbits:** investigação e migração suspensas **até depois da entrega da documentação do TG2**, salvo necessidade explícita e justificada. Não declarar defeito físico confirmado nem reutilizar resultados inconclusivos como prova.
5. **Objetivo imediato:** validar a V0.17 no ESP32 e integrar, por USB, um primeiro ensaio real com a interface Python, recuperação e salvamento no computador; atualizar o TG2 de acordo com o efetivamente testado.
6. **Circuito de ignição:** fisicamente desabilitado em todo teste de software; nenhuma tentativa de implementar ou habilitar ignição nesta etapa.

## 2. Evidências confirmadas e limites

### Firmware V0.17

- Arquivos principais: `Bancada_Empuxo_V0_17.ino`, `Armazenamento.h`, `ComandosArmazenamento.h`, `TransferenciaLoRa.h`. A pasta da V0.17 deve permanecer completa ao abrir na Arduino IDE.
- Implementa protocolo USB JSON por linha **versão 2**, a **115200 baud**; amostra formato 2 em **kgf**; armazenamento LittleFS interno **formato 3**; transmissão de ensaio armazenado por LoRa **BT/1**. A V0.17 é a fonte do contrato, não os diagnósticos da V0.18.
- No `setup()` da V0.17 consta `LittleFS.begin(false)`: inicialização **sem formatação automática**.
- Pinagem **declarada na V0.17 e compatível com os esquemas enviados**: HX711 DT=GPIO33 / SCK=GPIO32; LoRa CS=GPIO21, RST=GPIO14, DIO0=GPIO26; SPI SCK=GPIO18, MISO=GPIO19, MOSI=GPIO23; LEDs execução=GPIO4, LoRa=GPIO2.
- **141/141 testes nativos simulados** foram reproduzidos pela auditoria de 15/11/2026. Isto **não valida fisicamente** a gravação nem comprova uma compilação anterior da V0.17 para a placa real.
- A V0.18 foi carregada e executada fisicamente no ESP32, mas esta observação **não substitui** a compilação e os testes da V0.17 com LittleFS interno.
- Confirmar modelo/FQBN da placa, versão do core, bibliotecas e **esquema de partições** usado na V0.17 antes de upload; não alterar o layout nem executar `Erase All Flash`/upload de imagem de filesystem sobre ensaios existentes.

### Interface Python

- Baseline auditada: `Interface_Bancada_Etapa_06(1).zip` (Etapa 06). Possui interface demonstrativa, perfis, logs e histórico fictício; **não possui ainda cliente serial integrado**, aquisição/gráfico ao vivo reais nem salvamento/exportação de ensaios reais.
- A auditoria registrou **24/24 verificações de lógica Python** aprovadas, sem validar a janela visual ou comunicação real.
- Inconsistência localizada pendente: parser do cadastro local de perfis permite chave JSON duplicada, ao contrário do importador. Corrigir quando for oportuno, com teste de regressão; não bloquear a checagem de armazenamento do ESP32 por isso.
- Documentação pendente: `COMANDOS_E_TESTES.md` contém exemplo obsoleto do comando `ENVIAR`. O contrato real exige **`ensaio_id` explícito**.

## 3. Histórico da Flash externa — arquivado, não aplicar como baseline

**O que se observou durante a investigação:**

- Módulo físico instalado na protoboard; esquema especifica Flash CS GPIO13, LoRa CS GPIO21 e SPI 18/19/23; memória anunciada 16 Mbits (aprox. 2 MiB).
- Diagnóstico JEDEC original de somente leitura retornou **`EF 40 15`** em testes anteriores, inclusive após falhas de outros diagnósticos.
- Firmware V0.18 chegou a indicar `JEDEC_NAO_SUPORTADO` com `000000`; diagnóstico isolado da V0.18 também indicou `000000`; teste comparativo encontrou `FFFFFF` nos três caminhos avaliados.
- Mudança experimental do quarto argumento de `SPI.begin()` de GPIO21 para GPIO13 **não resolveu** a falha.
- A V0.18 foi entregue com resultados relatados de **217 testes nativos** (141 regressões e 76 adicionais), mas o funcionamento físico persistente do backend externo **não foi comprovado**.
- Esses resultados são **inconclusivos sobre a causa** (hardware, fiação, sinal, sequência SPI ou driver). **Não afirmar que a Flash externa está defeituosa.**

**Política de isolamento:** não carregar os diagnósticos experimentais, não reutilizar alterações de CS/driver/`SPI.begin` da V0.18 e não transportar para a V0.17 soluções tentativas do ramo externo. A memória externa pode permanecer fisicamente presente; antes de qualquer teste da V0.17, conferir que seu CS não interfira no SPI compartilhado, sempre com equipamento desligado para qualquer ajuste elétrico. Não presumir isolamento físico garantido.

## 4. Cuidados ao retornar ao firmware interno

1. Preservar o ZIP V0.17 original, a V0.18 experimental e os dados de ensaios existentes; criar um commit/tag de **decisão** no Git, sem sobrescrever o histórico.
2. Conferir a configuração de partições usada anteriormente. Carregar novo sketch pode preservar o LittleFS interno **se** o esquema de partições e o método de upload não alterarem sua região; isso deve ser verificado, **não presumido**.
3. Não executar opção `Erase All Flash`, `LittleFS.format()`, upload de imagem de filesystem, troca desnecessária de partição ou exclusão de ensaios. Primeiro consultar dados que já existirem; exportar/copy-back quando possível antes de qualquer mudança de layout.
4. Compilar e carregar a **V0.17 original sem refatorar**; registrar FQBN, versão do core e bibliotecas. Caso falhe, corrigir só o erro demonstrado, com diff pequeno.
5. Via USB: `IDENTIFICAR` para obter sessão, depois `STATUS` e `ESPACO`; verificar que o backend está funcional e dimensionado pela **partição interna real**, não pelos 2 MiB externos.
6. Antes de um ensaio, confirmar sensor, calibração e tara válidos; usar apenas cargas inertes e testes benignos. Criar um ensaio descartável somente após assegurar que não haverá perda de dados importantes. `INICIAR`/`FINALIZAR` são comandos JSON do protocolo 2, não texto simples.
7. Verificar gravação, encerramento, catálogo, recuperação integral, reinício do ESP32 e leitura do mesmo ensaio; comparar IDs, quantidade, índices e tempos. Não excluir automaticamente após copiar.
8. Separar claramente o que foi **IMPLEMENTADO**, **VALIDADO EM SOFTWARE** e **VALIDADO FISICAMENTE**.

## 5. Prioridades seguintes

| Ordem | Etapa | Critério de avanço |
|---|---|---|
| 1 | Retorno à V0.17 / build real | Build ESP32 registrado, serial operacional, LittleFS interno disponível sem formatação |
| 2 | Persistência física | Criar/fechar ensaio benigno, reiniciar, listar e recuperar amostras íntegras |
| 3 | Integração USB mínima | Interface descobre sessão, lê STATUS e gerencia erros/desconexões sem travar |
| 4 | Aquisição e gráfico | Tara/calibração confirmadas, início e final reais, amostras com tempos/índices do ESP |
| 5 | Histórico/exportação | Recuperação, salvamento local e abertura dos dados, CSV/Excel conforme escopo, sem exclusão automática |
| 6 | Documentação TG2 | Descrever arquitetura final, testes comprovados, limitações e evolução futura |
| Depois do TG2 | Flash externa e receptor BT/1 real | Trabalhos independentes; nenhum deles pode bloquear a integração USB essencial |

## 6. Organização dos chats

- **Coordenação do TCC (chat atual):** decisões, checkpoints, priorização, revisão de entregas.
- **Firmware ESP32 — Testes e Correções (Chat normal):** validar a V0.17, corrigir somente problemas reais, produzir evidências físicas. Não continuar automaticamente a sequência de testes externos A/B/C/D/E.
- **Firmware ESP32 — V0.18 Flash Externa (Work):** manter como registro histórico experimental. Suspenso.
- **Interface Python:** evoluções da GUI quando houver contrato serial validado.
- **Integração ESP32–Python:** abrir após a fase de validação da base.
- **Documentação TG2:** registrar somente fatos sustentados pelo código e pelos testes disponíveis.

**Modo de trabalho:** Chat para depuração localizada, planejamento e acompanhamento; Work para auditorias ou integrações grandes com múltiplos arquivos, somente quando agregarem valor. Entregar diffs e testes, evitando reescrever os arquivos à toa.

## 7. Pacote mínimo para o próximo chat

Anexar **(a)** este checkpoint, **(b)** o ZIP original da V0.17 e, se necessário, **(c)** esquema elétrico da placa. Não anexar diagnósticos experimentais V0.18, salvo quando houver retomada autorizada da Flash externa. A auditoria V0.17/GUI 06 pode ser consultada como referência de achados, mas os fontes atuais prevalecem.

### Mensagem de abertura sugerida

> Vamos validar a V0.17, que utiliza LittleFS **interno**, como nossa referência oficial para a entrega do TG2. A V0.18 e todos os diagnósticos da Flash SPI externa foram arquivados: não devem orientar correções atuais. Leia o checkpoint e o ZIP V0.17; preserve protocolos, pinagem, dados e ensaios. Primeiro confira compilação real, partições e inicialização do LittleFS **sem formatação**. Depois oriente testes físicos pequenos, um por vez. Não modifique a interface Python nesta etapa. Mantenha a ignição fisicamente desabilitada. Entregue versões, alterações, testes, resultados e limitações, distinguindo implementação, validação de software e validação física.

## 8. Hierarquia para evitar contaminação por tentativas anteriores

1. **Código e esquema reais da versão ativa (V0.17)**, seguidos de testes físicos reproduzíveis.
2. **Este Checkpoint 06**, que revoga a decisão anterior de usar a Flash externa **agora**.
3. Auditoria técnica da V0.17 e documentação compatível com o código real.
4. Checkpoints 01–05, V0.18 e diagnósticos SPI: **histórico arquivado**, sem autoridade sobre a implementação atual.

**Não apagar os registros históricos:** a “limpeza” consiste em **não herdar hipóteses, comandos experimentais ou decisões já revogadas**. No futuro, se a Flash externa voltar ao escopo, fazer novo checkpoint específico e reabrir essa investigação desde as evidências conservadas.
