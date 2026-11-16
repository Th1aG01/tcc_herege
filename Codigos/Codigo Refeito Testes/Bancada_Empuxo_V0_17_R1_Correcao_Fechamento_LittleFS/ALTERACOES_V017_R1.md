# V0.17.1 (R1) — Hipotese de correcao do fechamento LittleFS

Data: 2026-11-16
Referencia: V0.17 (Checkpoint 06). Pacote original preservado.

## Evidencia fisica anterior
- Ensaio FAA032865CC95E9F-1: 90 amostras; 6152 bytes (392 + 90*64), sem rodape de 56 bytes.
- FINALIZAR retornou FLASH_FECHAMENTO; CONSULTAR_ENSAIO retornou SEM_FECHAMENTO_VALIDO.
- Amostras iniciais e finais recuperadas com CRC valido; quatro finais permaneceram iguais apos reset.
- O erro ainda nao foi reproduzido em hardware com esta correcao.

## Hipotese tecnica e modificacao
Em encerrarEnsaio, a verificacao de arquivoEnsaio.size() vinha antes do flush final.
No Arduino-ESP32, a implementacao VFS faz fwrite() e consulta stat() para size().
Quando ainda existem bytes no buffer, size() pode nao coincidir com o numero
esperado de amostras, impedindo a tentativa de escrever o rodape.

Inserido arquivoEnsaio.flush() imediatamente antes da verificacao do tamanho,
sem alterar o layout de arquivo nem o protocolo de comandos. Continua sendo
necessario reabrir e verificar o rodape (mecanismo original mantido).

## Arquivos modificados
- Bancada_Empuxo_V0_17/Bancada_Empuxo_V0_17.ino: sincronizacao pre-checagem e firmware 0.17.1.
- validacao_nativa/test_firmware.cpp: expectativa de resposta IDENTIFICAR passa para 0.17.1.
- VERSAO.txt: descricao da revisao.
- ALTERACOES_V017_R1.md: esta nota nova.

## Testes e limitacoes
- Inspecao estatica: uma inclusao de flush antes de size; alteracao de versao; nenhuma alteracao em Armazenamento.h, ComandosArmazenamento.h, TransferenciaLoRa.h.
- 141/141 testes do ZIP original sao evidencias da V0.17, NAO da V0.17.1.
- Os testes nativos da V0.17.1 nao foram executados: dependencia ArduinoJson 7.4.2 ausente neste ambiente.
- Compilacao da V0.17.1 para ESP32 nao executada neste ambiente.
- Teste fisico da V0.17.1 NAO executado.
- Hipotese da causa: plausivel e apoiada na implementacao do VFS, ainda sem confirmacao experimental.

## Procedimento de validacao fisica sugerido
1. Manter ignicao fisicamente desabilitada e modulo SPI externo isolado; nao apagar Flash.
2. Antes de upload, confirmar Partition Scheme e Erase All Flash = Disabled; nao alterar layout.
3. Compilar V0.17.1 na Arduino IDE. Upload so apos conferir preservacao da particao LittleFS.
4. IDENTIFICAR (sessao nova) e ESPACO; CONSULTAR_ENSAIO do registro anterior antes de novo teste.
5. CONFIGURAR_CALIBRACAO com coeficiente provisorio, TARA sem carga e ensaio benigno curto.
6. FINALIZAR; confirmar ENSAIO_ENCERRADO concluido, fechamento_confirmado true,
   CONSULTAR_ENSAIO concluido, bytes = 392 + 64*N + 56.
7. Reiniciar e reler amostras; nenhuma exclusao automatica.
8. Se persistir falha, registrar mensagens e acrescentar diagnostico que diferencie
   tamanho inesperado, escrita incompleta do rodape e falha de verificacao pos-close.

**Nao afirmar que o problema foi resolvido antes do teste fisico.**
