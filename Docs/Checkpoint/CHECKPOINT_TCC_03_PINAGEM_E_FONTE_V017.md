# CHECKPOINT 03 — Confirmação de fonte e pinagem para V0.18

**Data:** 15/11/2026 (horário de São Paulo). **Objetivo:** consolidar evidências recebidas após auditoria. Não significa que a V0.18 foi implementada.

## Evidências recebidas

- ZIP: `Bancada_Empuxo_V0_17_Transferencia_LoRa(2).zip`, SHA-256 `8e3f904c3f3a25881d212b8fa7d5f427617c3544565aab7e0bd9b561b71b5f57`. O hash coincide com a baseline auditada.
- Desenho ESP32-30P (esquema elétrico) e desenho do header da Flash SPI externa. **São esquemas, não fotos da protoboard nem evidência de teste elétrico.**
- Flash externa instalada segundo relato do autor. CI efetivo (marcação e ID JEDEC) ainda não confirmado.

## Pinagem — confrontada com fonte V0.17

| Sinal | GPIO | Observação |
|---|---:|---|
| Flash CS | 13 | No esquema, ainda inexistente no código V0.17 |
| LoRa CS | 21 | Compatível entre fonte e esquema |
| LoRa RST / DIO0 | 14 / 26 | Compatível |
| SPI SCK / MISO / MOSI | 18 / 19 / 23 | Barramento comum, compatível |
| HX711 SCK / DT | 32 / 33 | Compatível |
| LED EXEC / LED LoRa | 4 / 2 | Compatível |
| Flash VCC / GND | 3V3 / GND | Desenho mostra 3,3 V; medir antes de energizar |
| Flash WP / HOLD | 3V3 via resistores independentes de 10 kΩ | Desenho; conferir ligação física |

A Flash e o LoRa DEVEM usar CS distintos. Ambos os CS devem permanecer altos quando seus dispositivos não estão selecionados. Considerar pull-up em FLASH_CS para boot/reset depois da inspeção física.

## Decisão de arquitetura

A memória Flash SPI externa de 16 Mbits (2.097.152 bytes nominais, se chip confirmado) será o **backend principal** dos ensaios da V0.18, substituindo o LittleFS interno apenas para novos registros. Dados anteriores mantidos na partição interna não migram automaticamente: preservar originais, exportar/recuperar antes se necessários. O firmware V0.17 não está compilado para a placa nem testado na Flash externa.

## Próxima etapa

1. Inspecionar marcação do chip; realizar somente leitura JEDEC 0x9F com esquemas e fiação conferidos.
2. Com chip/modelo identificado, implementar driver e sistema de arquivos/armazenamento externo no Work, usando o prompt atualizado.
3. Preservar o protocolo USB 2, formato binário 3, APIs de catálogo, transferência LoRa BT/1 e os 141 testes nativos como regressão.
4. Compilar para ESP32, testar com hardware sem ignição, confirmar gravação/leitura/reboot/exclusão em amostras descartáveis antes da integração Python.

**Gate físico:** nenhuma escrita, limpeza/format ou exclusão no chip externo sem identificação, confirmação de região segura e autorização explícita. O sketch `Diagnostico_Flash_JEDEC_ESP32.ino` contém somente leitura JEDEC, não grava/apaga a Flash externa.

## Status

- Fonte V0.17 autenticada pelo hash: **confirmado**.
- Pinagem fonte/esquema: **compatível**.
- Fiação real/marcação do CI: **pendente**.
- JEDEC e compilação ESP32: **pendente**.
- Migração V0.18: **não iniciada**.
