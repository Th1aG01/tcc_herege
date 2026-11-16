# Alterações V0.17 → V0.18

## Resultado

Novos ensaios passam a usar explicitamente a Flash SPI externa compatível com EF4015.
Falha desse backend é reportada e bloqueia aquisição; não redireciona para Flash interna.
Arquivos formato 3, comandos USB 2, metadados e transporte BT/1 foram mantidos.

Entrega com `FLASH_PERMITIR_ESCRITA=0`, sem formatação no firmware principal.
Existe preparação separada, mediante autorização serial e confirmação de chip inteiramente vazio.
Nenhuma ação física de escrita/apagamento foi realizada nesta entrega.

## Arquivos de produção

| Arquivo | Alteração e motivo |
|---|---|
| `Bancada_Empuxo_V0_18.ino` (renomeado de V0.17) | Versão, adaptador externo, CS antes do SPI, montagem sem criação automática, diagnósticos, verificação de sync/close e polling TX_DONE |
| `Armazenamento.h` | Troca localizada de File/LittleFS para ArquivoFlash/FlashEnsaios; estruturas, CRC e recuperação preservados |
| `ComandosArmazenamento.h` | Mesmo adaptador; backend/diagnóstico em ESPACO e recusa de exclusão em somente leitura |
| `TransferenciaLoRa.h` | Callback de ISR desativado em todas as fases, mantendo máquina BT/1 |
| `ConfiguracaoFlash.h` (novo) | CS13, SPI18/19/23, capacidade, ID esperado, clock e trava de escrita |
| `FlashNor.h` (novo) | Driver NOR mínimo com transações, timeout e verificação por releitura |
| `BackendExterno.h` (novo) | Adaptador de arquivos LittleFS; erros de sync/close; exclusividade na criação e espaço estimado |
| `RadioPolling.h` (novo) | Consulta/limpeza de TX_DONE no loop, evitando SPI na ISR |
| `src/littlefs/` (novo) | LittleFS 2.11.3 e licença, símbolos isolados e logs UART desativados |

O rádio permanece 915 MHz, SyncWord 145, potência 2 dBm e SPI 1 MHz.
HX711, LEDs, baud, coeficiente inicial e exigência de calibração/tara não mudaram.
Não foram adicionadas funções de ignição. Não houve alteração na interface Python ou receptor.

## Testes e documentação

Os 141 casos originais permanecem no runner. Adaptações dos testes:

- Caminho do sketch e expectativa de versão 0.18/backend externo.
- Configuração verifica também os dois novos modos OUTPUT de CS e seus níveis HIGH.
- Mock de arquivos fornece a mesma API com retorno de sync/close; diretório pré-existente
  na fixture, pois o boot real não cria mais diretórios.
- Injeção de falta de TX_DONE passa pelo bit IRQ simulado, não por remoção de callback.
- Asserções de callback esperam sempre nullptr, preservando cancelamento e transição RX/TX.

Essa suíte conserva falhas lógicas do protocolo usando arquivos em RAM; **não testa o
driver NOR**. A suíte adicional em `validacao_flash/` inclui o driver novo e o LittleFS
reais compilados, substituindo somente SPI/tempo/sensor/rádio pelo modelo de teste.
Sua simulação de rádio também gera transações SPI para verificar CS, mas não reproduz
a implementação completa nem a temporização física do SX127x.

`executar_testes.py` compila e executa as duas suítes e utilitários com sanitizadores.
As evidências originais de V0.17 são diferenciadas das novas em `evidencias/`.
Documentos anteriores foram preservados em `documentacao_base_v017/`; os novos documentos
descrevem a V0.18 e prevalecem para armazenamento e conclusão de TX.

`diff_V017_V018.patch` é um diff de revisão das mudanças de autoria própria, incluindo
fontes e testes; não inclui milhares de linhas upstream nem saídas JSON geradas.
O ZIP é a entrega completa, sem exigir aplicar o patch manualmente.

## Limitações e próxima etapa

- Compilação ESP32 bloqueada por core/toolchain não disponíveis no ambiente; ver log.
- Nenhuma medição de cadência, consumo de RAM na placa, escrita física ou persistência V0.18.
- Erase/commit síncronos podem causar pausas de aquisição; validar com a taxa HX711 real.
- Estimativa conservadora de espaço pode interromper um ensaio longo antes do limite nominal.
- Sem migração automática dos arquivos internos e sem novo campo ADC bruto no formato 3.

Próxima etapa: compilar com o ambiente Arduino da V0.17, verificar boot somente leitura,
preparar/habilitar a Flash apenas com autorização e executar dois ensaios curtos com reboot.
Após esse aceite, iniciar a integração USB com a interface Python.
