# TCC — Implementação V0.18: migração para memória Flash SPI externa de 16 Mbits

## Contexto e evidências

Bancada de empuxo BRAVO/Univap, microcontrolador ESP32 DevKit, célula HX711 e rádio LoRa; firmware atual V0.17, auditado no documento `AUDITORIA_TCC_V017_INTERFACE06(1).md`. O usuário confirmou que o módulo Flash CJMCU-2516 anunciado como 16 **Mbits** (aproximadamente 2 MB) já está instalado na protoboard e conectado fisicamente. A auditoria constatou que a V0.17 grava no **LittleFS da Flash interna do ESP32**, e **não** na Flash externa. Portanto, migrar de verdade o backend de armazenamento nesta etapa, preservando as funcionalidades existentes, a interface USB e LoRa.

A base fonte é o ZIP `Bancada_Empuxo_V0_17_Transferencia_LoRa(1).zip` e o relatório de auditoria. Se o ZIP não estiver acessível, não inventar código: solicitar/identificar a fonte antes de modificar.

## Restrições de hardware (essenciais)

1. Antes de definir driver e configuração, registrar **marcação do CI físico** e, se disponível, ler o identificador JEDEC por SPI usando uma rotina **somente leitura**. O nome do módulo não prova qual CI foi soldado.
2. Registrar ligação real dos pinos VCC, GND, CS, CLK, DI/MOSI, DO/MISO, WP e HOLD do módulo; confirmar VCC em 3,3 V e níveis compatíveis. Não inventar pinagem nem assumir que esquemas antigos refletem a protoboard atual.
3. Configuração LoRa declarada na V0.17 (segundo auditoria): CS=GPIO21, RST=14, DIO0=26; SPI SCK=18, MISO=19, MOSI=23; rádio 915 MHz, SyncWord decimal 145, 2 dBm, clock SPI 1 MHz. Confrontar esta configuração com **hardware conectado** antes de mudar. O usuário já usou GPIO5 historicamente para o CS do LoRa, logo é crucial conferir o circuito atual.
4. Quando Flash e LoRa compartilharem MOSI/MISO/SCK, **devem ter CS distintos**. Mesmo pino CS para os dois escravos simultaneamente é incorreto. Garantir deselegição e transações SPI corretamente gerenciadas; não alterar configurações do rádio sem motivo validado.
5. Se pinagem exata ou ID do chip não forem fornecidos, documentar claramente o bloqueio; é aceitável produzir diagnóstico de leitura JEDEC e uma integração com `FLASH_CS` configurável e sinalizada como não validada, mas não afirmar sucesso físico.

## Escopo da V0.18

1. Alterar **somente o firmware ESP32**, testes e documentação de firmware. Não modificar a interface Python nem o firmware do receptor LoRa. Não implementar nenhuma função de ignição, intertravamento motor ou alteração na saída de ignição; testar com ignição fisicamente desabilitada.
2. Projetar um backend dedicado à Flash NOR SPI externa, com um adaptador mínimo para as operações usadas na V0.17 (criação/abertura, escrita, leitura, flush/fechamento, enumeração, exclusão e espaço). Avaliar as opções reais de driver/filesystem compatíveis com o CI, o core Arduino-ESP32 e os limites de RAM/CPU. **Não presumir que `LittleFS.begin()` do core Arduino monte automaticamente o chip externo** — ele hoje monta partição interna. Se houver necessidade de mudança de API, alterar apenas os pontos necessários e documentar.
3. Usar o chip externo como backend de ensaios **principal e explícito**, não cair silenciosamente no LittleFS interno se o externo falhar. Reportar backend e falhas em `STATUS` e `ESPACO` com campos compatíveis ou extensão versionada/documentada. Não formate automaticamente flash em caso de erro; não apague ensaios ao inicializar.
4. Preservar a semântica de registros: arquivos/IDs por ensaio, layout binário formato 3 (cabeçalho 392 B, amostra 64 B, rodapé 56 B segundo a auditoria), CRC, identificação, paginação, recuperação, eventos e erros; preservar leitura bruta, timestamps e metadados, sem regravar/exportar em formato reduzido. Se não for tecnicamente possível preservar todos os bytes no backend escolhido, apresentar a incompatibilidade e a opção de migração antes de alterar o contrato.
5. Preservar comandos existentes (`IDENTIFICAR`, `STATUS`, `INICIAR`, `FINALIZAR`, `LISTAR_ENSAIOS`, `CONSULTAR_ENSAIO`, `LER_AMOSTRAS`, `EXCLUIR_ENSAIOS`, `EXCLUIR_TODOS`, `ESPACO`, `ENVIAR`, `CONSULTAR_ENVIO`, `CANCELAR` e outros já presentes), sessão/IDs, USB JSON e transporte LoRa BT/1. `ENVIAR` exige `ensaio_id` explícito.
6. Manter aquisição independente da conexão USB/LoRa. Nunca apagar automaticamente ensaios após transmissão ou recuperação. Bloquear aquisição de forma explícita quando não houver espaço ou a Flash externa estiver indisponível. Identificar registros incompletos sem destruí-los.
7. Considerar custo real de page program, block erase e catalogação na cadência do HX711; evitar bloqueios longos durante aquisição sempre que possível. Não declarar periodicidade garantida sem testes de temporização no hardware. Documentar estratégia de buffer/pré-apagamento e limites de perda em corte de energia.
8. Não usar mecanismos inseguros de acesso concorrente à SPI; verificar se o driver e LoRa usam transações e CS adequados. Preservar LEDs e rádio na configuração validada.
9. Não fazer refatoração estética ampla do `.ino`; introduzir somente abstração mínima exigida por armazenamento, com módulos claros quando justificável.

## Verificações requeridas

- Preservar o ZIP V0.17 intacto; entregar V0.18 separado, lista de arquivos modificados, diff/resumo de alterações, dependências/versões, configuração de placa/partição e instruções para compilação Arduino IDE.
- Estender os 141 testes nativos atuais (não os eliminar). Simular Flash presente/ausente, leitura JEDEC, erros de erase/program/flush, capacidade cheia, reboot e arquivo incompleto, corrupção e recuperação de blocos, exclusão protegida, múltiplos ensaios, armazenamento persistente, e LoRa trabalhando no mesmo barramento SPI.
- Compilar usando toolchain **ESP32 real** se disponível; registrar FQBN, versões e saída da compilação. Sem toolchain, declarar explicitamente que build ESP32 é pendente; testes nativos não substituem a compilação embarcada.
- Produzir teste físico **não destrutivo** de identificação JEDEC como primeiro teste; só depois de identificar e confirmar o chip, executar escrita/leitura/apagamento em região de teste autorizada que não contenha dados anteriores. Solicitar consentimento antes de operações que possam destruir conteúdo existente. Não realizar teste com motor/ignitor conectado.
- Testes físicos manuais: identificar Flash e LoRa, registrar 2 ensaios curtos com HX711, desligar e religar ESP, listar e ler ensaios com dados idênticos, inspecionar CRC, medir espaço livre, rejeitar memória cheia, transmitir ensaio com receptor quando estiver disponível e garantir que os arquivos permaneçam na Flash.
- Reexecutar regressões USB, aquisição, identificação, calibração/tara e transferência LoRa; registrar PASS/FAIL/BLOQUEADO e evidências.

## Entregáveis

1. ZIP do firmware ESP32 V0.18, com fontes e documentação.
2. `MIGRACAO_FLASH_EXTERNA.md`: chip efetivo, ligações, driver, backend, riscos, estratégia de preservação, capacidade em bytes e teste físico.
3. `VALIDACAO_V018.md`: resultados reais vs. simulados, comandos para reproduzir, restrições não verificadas e dependências.
4. Sumário objetivo de mudanças, regressões e passos restantes.

## Critério de parada

Não afirmar que a Flash externa foi integrada com sucesso só por passar testes nativos. Se o modelo real, os fios CS/pinagem, a biblioteca ou a compilação de placa forem desconhecidos, informar claramente o que impede a conclusão e produzir o máximo de material testável sem suposições inseguras. O objetivo é uma migração mínima, verificável e sem regressões, para só então iniciar a integração Python.


## ADENDO VALIDADO POR ESQUEMA E ZIP — 15/11/2026

O usuário forneceu a fonte exata `Bancada_Empuxo_V0_17_Transferencia_LoRa(2).zip` e dois desenhos elétricos do ESP32 e do conector de memória. O ZIP tem SHA-256 `8e3f904c3f3a25881d212b8fa7d5f427617c3544565aab7e0bd9b561b71b5f57`, **igual à baseline auditada**. O caminho do sketch dentro do ZIP é `Bancada_Empuxo_V0_17/Bancada_Empuxo_V0_17.ino`. Usar esta fonte fornecida, e não procurar versão mais antiga de outro chat.

**Pinagem conforme os desenhos (projeto, não medição física):**
- Flash SPI externa: `FLASH_CS = GPIO13` (D13), `SCK=GPIO18`, `MISO=GPIO19`, `MOSI=GPIO23`, `VCC=3V3`, GND comum; pinos `WP` e `HOLD` conectados a 3V3 através de resistores de 10 kΩ cada.
- LoRa: `LORA_CS=GPIO21`, `RST=GPIO14`, `DIO0=GPIO26`; mesmo SPI comum `SCK=18, MISO=19, MOSI=23`.
- HX711: `DT=GPIO33`, `SCK=GPIO32`; LEDs: execução `GPIO4`, rádio `GPIO2`. O código V0.17 coincide com a pinagem do esquema para LoRa/HX711/LEDs.
- **Não** usar CS LoRa GPIO5 de sketches históricos, nem dar à Flash o CS LoRa. Inicializar os dois CS como saída HIGH antes de abrir o barramento, verificar transações SPI de ambos e compatibilidade com interrupção assíncrona de `LoRa.endPacket(true)`.
- O nome CJMCU-2516 NÃO estabelece a identidade do CI soldado. O desenho não é foto do chip real nem teste de continuidade. Antes de escolher driver/programar, capturar **marcação do CI e JEDEC 0x9F**. Um sketch de leitura está disponível em `Diagnostico_Flash_JEDEC_ESP32.ino`. ID de exemplo do W25Q16JV 16 Mbits: EF 40 15. Qualquer resposta diferente exige identificação adicional, não manipular a Flash às cegas.

**Proteção de dados anteriores:** ensaios registrados na Flash interna da V0.17 permanecem na partição interna. A mudança para o armazenamento externo não os migra automaticamente. NÃO formatar nem excluir a partição interna, não apagar o chip externo e não importar conteúdo sem pedido específico, backup validado e estratégia documentada. O programa V0.18 não deve inicializar silenciosamente com outro backend. Caso haja arquivos internos importantes, planejar cópia segura separadamente, de preferência usando leitura USB da V0.17 antes da migração.

**Ordem obrigatória das entregas:**
1. Conferir chip/JEDEC e compatibilidade com biblioteca e core ESP32; caso não confirmado, entregar diagnóstico e parar antes de qualquer teste de escrita/apagamento.
2. Modelar o backend externo com isolamento dos comandos e sem modificar protocolo/arquivo formato 3; preservar LittleFS interno intacto.
3. Adicionar testes e compilar a placa real quando ferramenta disponível.
4. Só com autorização expressa para gravar/apagar regiões de TESTE consideradas vazias, executar teste físico de persistência; nunca apagar a memória inteira implicitamente.
5. Criar ZIP V0.18 e documentação; relatar exatamente o que está simulado, compilado e validado no hardware.

**Observações adicionais:** o esquema não mostra pull-up dedicado em FLASH_CS; avaliar se CS está em nível HIGH desde a energização/reset e se pull-up externo é necessário. Os resistores de 10 kΩ em WP e HOLD estão no esquema. A conexão em protoboard exige checagem física; não declarar o hardware validado a partir do desenho.


## ADENDO — DIAGNÓSTICO JEDEC REAL APROVADO (16/11/2026)

**Evidência física fornecida pelo usuário:** fotografia do Monitor Serial do ESP32 DevKit V1 (COM3) rodando `Diagnostico_Flash_JEDEC_ESP32.ino`, com a saída:

```text
=== Diagnostico JEDEC da Flash externa (somente leitura) ===
ID JEDEC: EF 40 15
ID compativel com Winbond W25Q16 (16 Mbits). Conferir marcacao fisica.
Nenhum dado foi escrito ou apagado na Flash externa.
```

O usuário apresentou também imagem do anúncio de compra `Módulo Memória Flash 16mb Cjmcu-2516`, cujo produto ilustrado tem CI marcado Winbond W25Q16. **A foto do anúncio não é foto do CI individual instalado**, mas o ID real `EF 40 15` sustenta a identificação de dispositivo SPI NOR compatível com Winbond W25Q16, capacidade **16 Mbits = 2.097.152 bytes**. A variante exata/revisão do chip não foi confirmada pela leitura JEDEC isoladamente.

**Mudança de condição:** o requisito de identificação JEDEC somente leitura foi cumprido no ESP32 e não é mais bloqueio de implementação. O trabalho no Work pode avançar **agora** para escolher driver compatível com `EF 40 15`, implementar backend externo e testes simulados, com os pinos definidos pelo esquema. Não exigir repetir o teste já realizado, salvo diagnóstico de falha. A marcação real do CI pode ser registrada posteriormente, se acessível, sem bloquear a implementação compatível com o JEDEC observado.

**Não extrapolar:** a comunicação JEDEC prova leitura simples no barramento SPI, mas **não prova** escrita, leitura de setores, apagamento, velocidade sob carga, compartilhamento com LoRa, boot/reset nem persistência. Não relatar esses testes como concluídos. Não alterar os pinos já verificados (`FLASH_CS=13`, `LORA_CS=21`, SPI `18/19/23`). Preservar `WP` e `HOLD` no estado alto esperado (segundo esquema via resistores de 10 kΩ) e manter todas as operações de Flash não destrutivas por padrão durante a fase inicial.

**Separação explícita de autorização:** o usuário autorizou **desenvolver o código** de migração. Isso não é autorização para apagar/formatar chips ou sobrescrever dados que possam existir. O Work deve preparar testes de gravação/leitura e de falhas em ambiente simulado. Testes físicos de escrita/apagamento só deverão ocorrer posteriormente com confirmação de região segura ou consentimento específico do usuário.

**Próxima entrega requerida:** V0.18 do firmware (ou relatório técnico de bloqueio demonstrado), com backend da Flash externa compatível com o ID lido, mudanças delimitadas, regressões, teste de SPI compartilhado com LoRa e evidências de compilação ESP32 se toolchain disponível. A interface Python continua inalterada nesta fase.
