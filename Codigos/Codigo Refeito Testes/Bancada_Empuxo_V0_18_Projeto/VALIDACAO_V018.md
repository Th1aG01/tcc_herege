# Validação V0.18 — resultados e limites

Data: 16/11/2026. Classificação: **IMPLEMENTADO / VALIDADO EM SOFTWARE NATIVO**.
**Build ESP32 e validação física V0.18: pendentes.**

## Evidências executadas

| Verificação | Resultado | Evidência |
|---|---|---|
| ZIP V0.17 corresponde à base auditada | PASS | SHA-256 `8e3f904c3f3a25881d212b8fa7d5f427617c3544565aab7e0bd9b561b71b5f57` |
| Reexecução da V0.17 original | PASS — 141/141 | `evidencias/baseline_141.log` |
| Regressões na V0.18 | PASS — 141/141 | `evidencias/regressoes_141.log`, `validacao_nativa/resultados.json` |
| Driver/filesystem, integração e manutenção | PASS — 76/76 | `evidencias/flash_externa.log`, `validacao_flash/resultados.json` |
| Compilação C/C++ nativa com warnings como erros | PASS | `evidencias/build_nativo.log` |
| ASan/UBSan durante cenários nativos | PASS — sem erros reportados | Mesmos logs e resultados por cenário |
| Build alvo ESP32 | BLOQUEADO | `evidencias/build_esp32.log`: plataforma `esp32:esp32` não instalada |
| Atualização do core pela rede | BLOQUEADO | `evidencias/arduino_index.log`: rede/DNS indisponível ao Arduino CLI |
| JEDEC físico anterior | VALIDADO PELO USUÁRIO | Checkpoint 04: EF 40 15; não repetido fisicamente aqui |
| Escrita/erase/persistência/rádio físico V0.18 | NÃO EXECUTADO | Seguir `VALIDACAO_FISICA.md` |

Total V0.18: **217 cenários/invocações aprovados**; a baseline 141/141 é separada,
não somada como se fosse novo teste da V0.18. Cada arquivo `resultados.json` contém o
nome, código de saída e mensagem de cada execução. Os `.jsonl` registram saídas simuladas;
as saídas dos utilitários de manutenção são texto, embora o runner use a mesma extensão.

## O que a suíte adicional executa

Ela compila o **mesmo FlashNor, BackendExterno, LittleFS 2.11.3, formato de arquivos,
máquina USB e transferência** do firmware. A NOR de 2 MiB em RAM implementa JEDEC,
status/WEL, fronteira de página, bitwise AND em program, setor de 4 KiB e falhas.
Cada transação verifica seleção exclusiva de CS e liberação do barramento.

| Grupo | Verificação |
|---|---|
| Presença/identidade | Flash ausente, JEDEC diferente, busy permanente, boot em chip FF e conteúdo desconhecido; zero programa/erase |
| Driver | Programa cruzando quatro páginas, leitura exata, erase verificado; falhas de programa, erase, WEL, proteção, limites e sync |
| Somente leitura | Recusa de criação, exclusão, mkdir e aquisição; bytes e contadores de mutação inalterados |
| Persistência entre processos | Imagem NOR de 2 MiB; dois ensaios concluídos e terceiro sem rodapé; segundo processo lista, confere CRC/valores e não escreve |
| Catálogo/integridade | Múltiplos arquivos, exclusão seletiva/protegida, criação exclusiva sem truncamento, corrupção de amostra detectada |
| Recuperação de metadados | Corrupção de um superbloco e montagem pela cópia válida preservando o ensaio |
| Falhas operacionais | USB bloqueada sem interromper gravação; falha real de program durante flush interrompe ensaio e preserva o anterior |
| Capacidade | Preenchimento até erro do LittleFS, recusa de novo ensaio e arquivo anterior preservado |
| SPI/LoRa | Leitura Flash durante TX simulado, polling, ACK/CONFIRMAR, remontagem byte a byte do arquivo BT/1; original preservado e CS exclusivos |
| Cortes | 24 posições de corte na sequência de programa/erase, cada uma com leitura por novo processo: 48 invocações; ensaio anterior legível e boot sem mutações |
| Utilitários | Diagnóstico não escreve; preparação exige frase exata; entrada longa/inválida não autoriza; chip com um byte ocupado é recusado; repetição não reformata |

Os cortes alteram parcialmente a operação em curso e interrompem a resposta do chip.
Não são uma cobertura exaustiva de todas as possíveis falhas elétricas ou todos os bytes
de corte. O teste de SPI usa modelo do rádio, não RF real nem execução completa da LoRa.cpp.
O modelo do HX711 e o relógio simulado não comprovam latência, taxa de amostragem,
calibração física, desempenho ou robustez elétrica.

## Reproduzir

Linux/WSL, Python 3, gcc e g++, na raiz extraída:

```bash
python3 executar_testes.py
```

O ArduinoJson 7.4.2 para testes está incluído, assim como LittleFS e suas licenças.
O script usa `-Wall -Wextra -Werror -pedantic`, AddressSanitizer e UndefinedBehaviorSanitizer.
LeakSanitizer é desabilitado (`detect_leaks=0`) pela limitação de ptrace do ambiente;
portanto, **não houve validação de vazamentos com LSan**. ASan/UBSan ficam ativos.
Não há portas seriais nem acesso ao hardware nesses testes.

## Dependências e compilação embarcada

| Item | Versão/estado |
|---|---|
| Arduino CLI disponível | 1.6.0; usado na tentativa de build |
| FQBN solicitado | `esp32:esp32:esp32` |
| Core Arduino-ESP32 | Não instalado neste ambiente; versão da bancada não fornecida |
| Compilador Xtensa | Não disponível; não houve geração de ELF/binário ESP32 |
| Tabela de partições | Não alterada pelo projeto; manter a usada na V0.17 e registrar seu nome |
| LoRa | 0.8.0 de Sandeep Mistry; fonte/API consultada para TX_DONE e SPI |
| HX711 | 0.7.5 de bogde; dependência indicada para IDE |
| ArduinoJson | 7.4.2; compilado nativamente |
| LittleFS | 2.11.3; compilado nativamente e incluído no sketch |

Tentativa real:

```bash
arduino-cli --config-file build_tools/arduino-cli.yaml compile \
  --fqbn esp32:esp32:esp32 V018/Bancada_Empuxo_V0_18
```

O log termina informando que a plataforma não está instalada. Não foi um erro de
compilação de uma linha do firmware, mas também **não é evidência de que o build da
placa passa**. Acesso às fontes via download não disponibilizou automaticamente o core
e suas ferramentas para o Arduino CLI. Nenhuma versão de core foi inventada como validada.

## Revisão de preservação

- Estruturas 392/64/56 B e verificações estáticas mantidas.
- Sem formatação no boot, sem inclusão do LittleFS interno, sem fallback e sem GPIO de ignição.
- Formatação somente no utilitário com autorização serial e varredura integral FF.
- Leitura de arquivos existentes, sessão, deduplicação, calibração/tara, paginação,
  cancelamento, proteções de exclusão, retransmissões e confirmação remota exercitados.
- Rádio/LEDs/pinos preservados; conclusão de TX mudou para polling para serializar o SPI.
- Arquivos originais não são removidos após transmissão/recuperação.

Critério de aceite físico continua aberto. Não usar estes PASS nativos como aprovação
para um ensaio com motor ou como resultado experimental na documentação acadêmica.
