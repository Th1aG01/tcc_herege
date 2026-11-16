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
