# Formato provisório de ensaio — schema_version 1

Este formato é do arquivo local. Não define o pacote transmitido por LoRa.

CSV em UTF-8 com BOM, delimitador `;`, ponto decimal. Uma linha por amostra.

| Campo | Significado |
|---|---|
| schema_version | 1 nesta entrega |
| session_id | Identificador da sessão, igual ao JSON |
| source | Sempre SIMULATED nesta entrega |
| seq | Índice inteiro consecutivo, começando em zero |
| t_us | Tempo da amostra em microssegundos desde o início; estritamente crescente |
| raw_counts | Contagem inteira simulada, entre -8388608 e 8388607 |
| calibration_id | Identificador da calibração presente no JSON |
| force_N | (raw_counts - offset_counts) × newtons_per_count |
| force_kgf | force_N / 9.80665 |

O JSON guarda versão do aplicativo, nome, observações, identificador, horários UTC do computador, parâmetros simulados e calibração. `t_us` é o tempo da fonte, não o horário de chegada de mensagens nem o relógio UTC.

Estados: `recording` enquanto grava; `completed` quando termina a duração prevista; `stopped` quando o usuário encerra; `error` quando uma falha é registrada. `sample_count` só é preenchido ao encerrar. A escrita do JSON usa arquivo temporário e substituição, reduzindo o risco de metadados parcialmente atualizados.

Ao abrir, o programa confere cabeçalho, origem, identificação, sequência, limites de contagem, ordem temporal, conversões e quantidade final de amostras. Registros `recording` podem ser abertos com aviso quando as linhas existentes são válidas. Linhas ausentes, duplicadas ou inconsistentes são recusadas; não há interpolação automática. CSV e JSON precisam permanecer juntos.

Os dois arquivos não são uma transação única. Falhas de energia ou disco ainda podem produzir uma sessão incompleta. Preserve os originais caso haja erro de leitura.
