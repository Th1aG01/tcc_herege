# Bancada de Empuxo — Firmware V0.18

Data: 16/11/2026, São Paulo. Base: V0.17, SHA-256 do ZIP original
`8e3f904c3f3a25881d212b8fa7d5f427617c3544565aab7e0bd9b561b71b5f57`.

**IMPLEMENTADO e testado nativamente. Compilação ESP32 e validação física da V0.18 pendentes.**
O Checkpoint 04 registra leitura JEDEC física `EF 40 15` anterior a esta implementação.
Isso não valida escrita, apagamento, persistência ou temporização deste firmware.

## Primeiro uso

1. Leia `MIGRACAO_FLASH_EXTERNA.md` e `VALIDACAO_FISICA.md`.
2. Na Arduino IDE, abra `Bancada_Empuxo_V0_18/Bancada_Empuxo_V0_18.ino`.
3. Instale **ArduinoJson 7.4.2**, **LoRa 0.8.0 de Sandeep Mistry** e
   **HX711 Arduino Library 0.7.5 de Bogdan Necula**. LittleFS externo está incluído em `src/`.
   Não instalar outra biblioteca LittleFS para tentar montar esta memória.
4. Placa-alvo: ESP32 Dev Module (`esp32:esp32:esp32`), ESP32 clássico/DevKit V1.
   Manter a **mesma versão do core, tamanho de Flash interna e tabela de partições usados na V0.17**.
   Esses valores locais não vieram nos anexos; registre-os na validação. Não escolher uma nova
   partição por suposição. **Erase All Flash Before Sketch Upload: Disabled**; não usar
   ferramentas de upload de filesystem nem `erase_flash`.
5. Compile antes do upload. O pacote não contém binário ESP32 validado.
6. A configuração entregue é `FLASH_PERMITIR_ESCRITA 0`, em `ConfiguracaoFlash.h`.
   Boot identifica e monta sem criar diretórios, formatar, apagar ou migrar dados.
   `INICIAR` e exclusões ficam bloqueados em somente leitura. Leituras e transmissão
   de arquivos válidos existentes continuam disponíveis.
7. Para habilitar gravação operacional, após autorização/verificação da memória,
   altere **somente** essa definição para `1` e recompile. Não mudar GPIO/radio/calibração.

Uma Flash vazia não contém LittleFS: `FLASH_INICIALIZACAO`/`FS_NAO_MONTADO` é esperado.
Há um utilitário separado `ferramentas/PrepararFlashVazia`, que **só escreve após frase de
autorização serial e varredura integral confirmando todos os bytes FF**. Ele não recupera
chips parcialmente inicializados nem aceita forçar formatação de conteúdo desconhecido.
Não é necessário executá-lo em uma memória já preparada.

## Conteúdo

- `Bancada_Empuxo_V0_18/`: sketch completo, driver, adaptador e LittleFS.
- `MIGRACAO_FLASH_EXTERNA.md`: arquitetura, preservação, capacidade e limitações.
- `VALIDACAO_V018.md`, `VALIDACAO_FISICA.md`: evidências e roteiro de bancada.
- `ALTERACOES_V018.md`, `diff_V017_V018.patch`: mudanças em relação à base.
- `validacao_nativa/`: os 141 cenários originais mantidos, com adaptações documentadas.
- `validacao_flash/`: driver/LittleFS reais com SPI NOR simulada, falhas e reboot.
- `evidencias/`: logs de execução e tentativa de compilação ESP32.
- `documentacao_base_v017/`: documentação original preservada, **histórica**;
  as referências a Flash interna, callback TX_DONE e resultados antigos são substituídas
  pelas definições desta V0.18. Os contratos USB 2, BT/1 e formato 3 continuam sendo a base.
- `terceiros/ArduinoJson/`: dependência dos testes nativos, com licença.

## Testes locais

Em Linux/WSL, com Python 3, gcc e g++:

```bash
python3 executar_testes.py
```

Usa ASan/UBSan e não abre portas seriais. Gera executáveis nativos e atualiza os logs
no diretório extraído. Esses executáveis **não são firmware para o ESP32**.

Nenhuma interface Python ou firmware do receptor foi alterado. Não foi feita
migração dos arquivos internos da V0.17. Ignição deve permanecer fisicamente desabilitada.
