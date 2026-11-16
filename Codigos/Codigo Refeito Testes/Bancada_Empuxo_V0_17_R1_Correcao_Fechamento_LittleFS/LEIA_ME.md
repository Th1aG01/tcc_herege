# Bancada de Empuxo V0.17 — transferência LoRa de ensaio salvo

Base: V0.16, sem modificações locais informadas. Alteração somente no ESP da bancada. USB a **115200 baud**, JSON por linha/protocolo 2, pinagem e configurações de rádio preservados. Unidade sempre kgf. Armazenamento continua no **LittleFS interno**, formato 3 compatível com V0.16.

Abra **Bancada_Empuxo_V0_17/Bancada_Empuxo_V0_17.ino** na Arduino IDE. Mantenha os **três .h na mesma pasta**: Armazenamento.h, ComandosArmazenamento.h e TransferenciaLoRa.h. Não copie apenas o .ino. Use sua placa, porta e esquema de partições atuais; não envie uma imagem de filesystem sobre os ensaios existentes.

Bibliotecas: HX711 de bogde, LoRa de Sandeep Mistry, ArduinoJson 7.x e SPI/LittleFS do core ESP32. O arquivo novo TransferenciaLoRa.h concentra a transferência; os dois cabeçalhos de armazenamento foram preservados byte a byte.

## O que mudou

- ENVIAR exige o identificador de um ensaio concluído. Valida seus registros e transmite o arquivo completo, incluindo metadados, coeficiente, tara interna, amostras e encerramento.
- CONSULTAR_ENVIO informa identificação, estado, blocos ACK, amostras cobertas, tentativas e resultado. CANCELAR pode receber o identificador da transferência para evitar cancelar outra operação.
- Pacotes BT/1 binários de até 218 bytes, blocos de 128 bytes, CRC por pacote e CRC do arquivo inteiro. Metadados podem ocupar vários blocos.
- Confirmações por bloco, pedidos de repetição, timeout e limites de tentativas. ENVIO_LOCAL_CONCLUIDO não é sucesso remoto; somente ENVIO_CONFIRMADO confirma a declaração de recebimento integral pelo receptor.
- Arquivo original protegido durante preparação, TX e espera; nunca apagado ou regravado pela transferência.
- LED amarelo aceso somente durante TX local, apagado na preparação e espera. LED verde continua associado à aquisição real.

Aquisição e envio completo são mutuamente exclusivos. O LoRa ao vivo continua separado, em JSON, sem ACK e sem garantir todas as amostras; a gravação guarda cada amostra aceita independentemente do rádio. Não há acionamento de motor nesta etapa.

## Testar esta entrega

Comece por **TESTES_LORA.md**: há comandos para copiar no Monitor Serial e resultados esperados mesmo sem receptor compatível. **COMANDOS_E_TESTES.md** mantém o roteiro de calibração, aquisição, catálogo e recuperação USB. **PROTOCOLO_USB.md** documenta os comandos; **PROTOCOLO_LORA.md** especifica o que o receptor deverá implementar; **FORMATO_E_RECUPERACAO.md** descreve os arquivos.

O receptor antigo não confirma BT/1. Seu firmware e a interface Python não foram alterados. Os arquivos V0.16 continuam utilizáveis; o antigo /ensaio.bin permanece reconhecido como legado e preservado, sem interpretação automática como formato 3.

**Validação executada:** consulte VALIDACAO.md e os resultados nativos. Compilação completa para ESP32, upload, DIO0/TX_DONE, alcance, recepção e LEDs físicos dependem do hardware. As simulações não substituem essas verificações.

O pacote não contém histórico nem versões anteriores. A pasta validacao_nativa é opcional para você: guarda testes e resultados, não precisa ir para a placa. SHA256SUMS.txt permite conferir integridade dos arquivos; VERSAO.txt identifica esta entrega.
