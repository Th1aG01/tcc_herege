# CHECKPOINT 04 — Flash externa identificada por JEDEC

**Data:** 16/11/2026 (referência São Paulo). **Etapa:** pré-implementação firmware V0.18.

## Evidências novas

- O usuário executou `Diagnostico_Flash_JEDEC_ESP32.ino` no ESP32 DevKit V1, com monitor serial COM3, 115200 baud.
- Resultado real exibido: **`ID JEDEC: EF 40 15`**, seguido de aviso de compatibilidade com Winbond W25Q16 (16 Mbits). O diagnóstico não escreveu nem apagou memória.
- O anúncio de compra mostrado pelo usuário é `Módulo Memória Flash 16mb Cjmcu-2516`. A foto comercial mostra um componente Winbond W25Q16, mas **não comprova a marcação do chip específico instalado**.
- Interpretação: resposta JEDEC real compatível com W25Q16, SPI NOR 16 Mbits = 2.097.152 bytes (2 MiB) nominais. A variante exata ainda não foi identificada independentemente por marcação do componente.

## Decisão

O requisito de detecção JEDEC está **cumprido** e permite iniciar no Work a implementação do backend externo de armazenamento da V0.18 sem inventar chip ou pinos. A Flash externa será o backend principal de novos ensaios.

**Pinagem do esquema:** Flash CS 13; LoRa CS 21, RST 14, DIO0 26; SPI SCK 18, MISO 19, MOSI 23; HX711 DT 33, SCK 32; LEDs execução 4, LoRa 2; Flash VCC 3V3, WP/HOLD altos via 10 kΩ.

## Ainda não validado

- Escrita, leitura de dados, erase, persistência após reboot/corte de energia.
- Execução simultânea/cooperativa com LoRa no mesmo SPI.
- Compilação/execução do firmware V0.18 para ESP32.
- Continuidade dos 141 testes nativos após migração e correspondência com a GUI Python.
- Medição elétrica da fiação/protoboard e marcação exata do chip real.

## Regras para implementação

1. Usar V0.17 autenticada pela auditoria como baseline, manter o original intacto.
2. Preservar o formato binário de ensaios, CRC, catálogo, operações de USB e LoRa BT/1.
3. Não migrar/apagar dados LittleFS internos automaticamente; não formatar Flash externa por padrão.
4. Tratar falha da Flash externa como erro explícito e não usar Flash interna como fallback silencioso.
5. Desenvolver código e testes simulados agora; aguardar autorização para operações físicas potencialmente destrutivas.
6. Validar SPI compartilhado sem selecionar os dois chips simultaneamente; preservar pinos do esquema.
7. Manter ignição fisicamente desabilitada nos testes.

**Próximo marco:** V0.18 implementada + regressões e build, seguidos de validação física segura. Depois, iniciar integração USB ↔ Python.
