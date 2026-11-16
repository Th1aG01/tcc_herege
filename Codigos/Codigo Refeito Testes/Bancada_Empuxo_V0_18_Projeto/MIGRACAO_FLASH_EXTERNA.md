# Migração para Flash SPI externa — V0.18

## Identificação e ligações

Checkpoint 04: o usuário já mediu JEDEC **EF 40 15**. Dispositivo compatível com
Winbond W25Q16, 16 Mbits = **2.097.152 bytes (2 MiB)**. A variante/revisão exata do
CI instalado não foi confirmada por marcação física. O driver aceita exclusivamente esse ID.
O esquema confirma o projeto dos fios, não continuidade, tensão ou montagem física.

| Sinal | Conexão |
|---|---|
| Flash VCC / GND | 3V3 do ESP32 / GND comum |
| Flash CS | GPIO13 |
| Flash CLK / DI / DO | GPIO18 / GPIO23 / GPIO19 |
| Flash WP / HOLD | Altos, previstos via resistores de 10 kΩ a 3V3 |
| LoRa CS / RST / DIO0 | GPIO21 / GPIO14 / GPIO26 |
| SPI comum SCK / MISO / MOSI | GPIO18 / GPIO19 / GPIO23 |
| HX711 DT / SCK | GPIO33 / GPIO32 |
| LED execução / LoRa | GPIO4 / GPIO2 |

Os dois CS são configurados como saída e colocados HIGH antes de `SPI.begin()`.
Isso não garante os níveis durante energização/reset: o esquema não mostra pull-up
dedicado em CS. Conferir esse ponto eletricamente antes de validar partidas/reboots.
Não alterar WP/HOLD, resistores ou GPIO a partir apenas dos testes simulados.

## Escolha do backend

Foi escolhido **LittleFS upstream 2.11.3 com callbacks de bloco para um driver SPI NOR
dedicado**. Preserva arquivos, diretórios, enumeração, seek e commit recuperável, sem
inventar um catálogo NOR próprio ou converter os ensaios em registros reduzidos.

`LittleFS.begin()` do Arduino monta armazenamento interno. Ele foi removido do caminho
de produção. A integração via partição/VFS do core exigiria adaptação dependente da
versão do ESP-IDF para um chip externo; aqui, a API pequena é independente dessa VFS.
Não foi alegado que outras bibliotecas fossem incompatíveis: a escolha minimiza a
dependência de APIs de montagem específicas de core e permite testar os callbacks reais.

Fluxo: comandos/aquisição → `ArquivoFlash`/`FlashEnsaios` → LittleFS incluído →
`FlashNor` → SPI → CS13. Nenhum acesso ao backend interno, nenhum fallback.
O adaptador cobre criar exclusivamente, abrir, ler, escrever, seek, tamanho, sync,
close, diretórios, enumeração, exclusão e espaço. Uma tentativa de criação em nome
existente usa `LFS_O_EXCL`, nunca truncamento, inclusive se a consulta anterior falhar.

O LittleFS incluído tem símbolos públicos prefixados por macros para não colidir com
uma implementação eventualmente vinculada pelo core. Logs de depuração do LittleFS
foram desativados para preservar a UART JSON. Fontes e licença upstream estão incluídas;
as únicas adaptações upstream são o include de `bancada_prefix.h` nos dois headers.

## Driver

- SPI modo 0, MSB primeiro, **1 MHz**; endereçamento de 24 bits.
- Comandos: JEDEC 9F, status 05/35, leitura 03, Write Enable 06, Page Program 02,
  Sector Erase 20. Não há chip erase, escrita de status, desbloqueio ou reset do chip.
- Páginas de 256 bytes e setores de 4096 bytes, 512 setores no total.
- Escritas divididas nas fronteiras de página; validação de endereço e transições 1→0.
- Verifica WEL, busy e proteções BP/CMP; não remove proteção existente.
- Relê para verificar cada programação e cada setor apagado. Falha de I/O bloqueia
  novas escritas até reinicialização/diagnóstico; não tenta reparar apagando conteúdo.
- Esperas limitadas: 20 ms para programa, 600 ms para erase/busy. `delay(1)` ocorre
  com CS alto e fora da transação. Não manter interrupções desabilitadas durante erase.

O datasheet de referência W25Q16JV Rev. F informa máximos de 3 ms de page program e
400 ms de erase 4 KiB. Esses são parâmetros do documento, **não medições do chip instalado**.

## Segurança dos dados e autorização

O firmware principal não possui caminho de formatação. `begin(false)` apenas identifica
e monta; `begin(true)` também não formata. Falha de montagem preserva o chip.
O diretório `/ensaios` deve existir; boot não o cria. Uma memória vazia fica indisponível
até preparação explícita separada. A configuração entregue também bloqueia todas as
operações de escrita; leitura, STATUS e diagnóstico não dependem de habilitá-las.

O utilitário de preparação tem uma rotina de formatação compilada apenas nele.
Seu boot somente lê; a frase de autorização deve ser recebida integralmente pela serial.
Antes de chamar `lfs_format`, verifica **todos os 2 MiB** como FF e exige JEDEC correto.
Se houver qualquer conteúdo, inclusive um filesystem parcialmente criado, recusa sem
apagar. Não é ferramenta de restauração/limpeza. Uma interrupção nessa preparação pode
exigir um procedimento posterior de recuperação autorizado; não repetir às cegas.

Os arquivos internos da V0.17 permanecem fora deste backend. O upload deve manter
partições e a opção de não apagar toda a Flash interna; o firmware sozinho não controla
as escolhas da ferramenta de upload. Fazer backup via V0.17 antes, se houver dados importantes.
Não há importação automática nem exclusão após recuperação, transmissão ou exportação.

## Formato e contratos preservados

Cabeçalho **392 B**, amostra **64 B**, rodapé **56 B**, CRCs, IDs, metadados, timestamps,
calibração e kgf permanecem sem mudança estrutural. `static_assert` verifica tamanhos e
offsets. JSON USB permanece protocolo 2; transferência binária permanece BT/1 e requer
`ensaio_id`. Os bytes recuperados e transmitidos são os bytes originais do arquivo.

Precisão sobre a base: o formato 3 da V0.17 armazena empuxo calculado, coeficiente e tara
no cabeçalho, mas **não possui campo inteiro de ADC bruto por amostra**. Esta etapa não
inventou esse campo nem alterou o layout. Preservar dados significa preservar o que a
base efetivamente registrava; uma futura gravação explícita do ADC exigirá outro contrato.

Extensões aditivas:

| Resposta | Campos |
|---|---|
| STATUS.armazenamento | `tipo=LittleFS_externo`, `diagnostico`, `jedec` numérico, `escrita` |
| ESPACO | `backend=LittleFS_externo`, `diagnostico`, `jedec`, `escrita`, `erro_fs`, demais campos existentes |
| Erros de origem Flash | `backend`, `diagnostico`, `jedec`, `erro_fs`, mantendo código/fase/ok existentes |

JEDEC numérico EF4015 = 15679509. `pronto` informa montagem/diretório; não implica permissão
de escrita nem validação física. `escrita` informa a permissão atual considerando falha
do driver. Sem montagem, ESPACO retorna erro explícito com identificação do backend,
em vez de capacidade fictícia. `FLASH_SOMENTE_LEITURA` bloqueia início/exclusão;
`FLASH_FLUSH` interrompe aquisição quando a sincronização falha. `erro_fs` retém o último
erro negativo LittleFS nesta sessão, podendo refletir uma operação anterior já encerrada.

## SPI e rádio

LoRa preservado: 915 MHz, SyncWord 145, 2 dBm, SPI 1 MHz e mesmos pinos.
Na LoRa 0.8.0, `handleDio0Rise()` lê/escreve registradores SPI antes do callback do usuário.
Somente tornar o callback curto não evita concorrência com a Flash.

A V0.18 mantém `endPacket(true)`, mas não registra callback de ISR. Consulta o bit TX_DONE
do registrador IRQ_FLAGS (12h) no loop e limpa apenas esse bit. RX continua por polling.
Assim, Flash, polling e chamadas da LoRa são sequenciais na mesma task. Não adicionar
`onReceive`, `onTxDone`, outra task SPI ou ISR SPI sem reavaliar essa exclusão.
DIO0 mantém a ligação física e a configuração da biblioteca, mas não é usado para
interrupção nesta versão. LED amarelo acompanha TX local observado pelo loop, incluindo
o atraso possível de uma operação síncrona; não representa confirmação remota.

## Cadência, espaço e corte de energia

Caches LittleFS: 256 B leitura + 256 B programa + 64 B lookahead, e 256 B por arquivo,
além das estruturas LittleFS, objetos e buffers já existentes. Isso não é uma medição
de RAM total/stack do ESP32. Não há um buffer de aquisição assíncrono nem pré-apagamento
de toda a memória; a alocação/apagamento é feita sob demanda pelo filesystem.

Sync do cabeçalho antes de aquisição, sync aproximadamente a cada 1000 ms no loop e
sync/close no encerramento. Agora os retornos são verificados. Só um fechamento relido
e consistente permite reportar ensaio concluído; caso contrário, fica incompleto.

Contabilidade: fora de escrita, `lfs_fs_size` conta blocos; durante o escritor, o cache
usa estimativa conservadora (um setor inicial + duas vezes os bytes escritos), sem
varredura por amostra. Fechar/excluir invalida o cache. Uma exclusão concorrente de outro
arquivo pode provocar nova varredura; medir essa condição se usada durante aquisição.
Os valores de ESPACO durante escrita são estimados, não bytes exatos de payload.
A margem permanece 8192 B. A estimativa pode encerrar um ensaio longo antes do limite
físico (aproximadamente metade do espaço então livre); após fechamento/reconsulta o
espaço real fica novamente disponível para novos ensaios. Nenhum ensaio é apagado para liberar espaço.

Um único arquivo não alcança os 2 MiB nominais: há metadados, ponteiros, blocos livres
de operação e margem. O teto teórico ignorando tudo isso seria 32.761 amostras de 64 B
com cabeçalho/rodapé; não é capacidade operacional garantida.

**Limitação relevante:** erase síncrono pode bloquear o loop por centenas de ms, além
de verificação/commit. Durante isso podem ocorrer conversões HX711 não capturadas;
não há garantia de 10 ou 80 amostras/s sem medição. Também pode atrasar RX/ACK e LEDs.
O laço continua independente de USB/receptor, mas não é aquisição em tempo real garantida.
Não foi introduzida uma segunda task/buffer sem testes físicos que justifiquem a complexidade.

Em corte, podem perder-se os dados posteriores ao último sync concluído. A janela nominal
é cerca de 1 s, **mais atrasos do loop/commit**, sem limite temporal rígido validado.
O LittleFS protege commits anteriores; falha elétrica e corrupção física arbitrária
podem exceder o modelo. Reboot identifica arquivos sem rodapé como incompletos e não
os finaliza, regrava, remove nem retoma aquisição automaticamente.

## Referências técnicas consultadas

- [LittleFS 2.11.3](https://github.com/littlefs-project/littlefs/tree/v2.11.3): fontes incluídas.
- [LoRa 0.8.0 — fonte](https://github.com/sandeepmistry/arduino-LoRa/blob/0.8.0/src/LoRa.cpp).
- [W25Q16JV Rev. F — Winbond](https://www.winbond.com/resource-files/w25q16jv%20spi%20revf%2005092017.pdf).
- [HX711 0.7.5](https://github.com/bogde/HX711/tree/0.7.5).

Fontes usadas para implementação/API, não como evidência de validação da placa do usuário.
