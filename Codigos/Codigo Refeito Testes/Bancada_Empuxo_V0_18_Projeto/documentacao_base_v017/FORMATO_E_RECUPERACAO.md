# Armazenamento e recuperação — formato 3

## Arquivos e identidade

LittleFS na flash interna do ESP32; a memória SPI externa não está integrada por esta entrega. Cada ensaio novo é um arquivo `/ensaios/<ensaio_id>.ben`. O ID combina a sessão aleatória do boot e contador crescente. Antes da criação, verifica-se se o caminho já existe; colisões avançam o contador, no máximo 32 tentativas por pedido, sem abrir um caminho existente para escrita. O firmware tem um único escritor no loop, sem outro agente modificando o filesystem.

Nome, data e observações não entram no caminho. IDs aceitam somente letras ASCII, números, `_` e `-`, até 31 caracteres, impedindo caminhos arbitrários. A própria gravação mantém o ID no cabeçalho e em cada amostra.

O catálogo é derivado do diretório; não há um arquivo de índice cuja perda torne todos os ensaios invisíveis. A inicialização apenas monta LittleFS sem formatar e prepara o diretório, sem truncar arquivos. Listagem e consulta inspecionam os registros sob demanda. Início de ensaio novo nunca faz limpeza automática dos anteriores. A exclusão de todos só alcança arquivos `.ben` com IDs válidos em `/ensaios` e o registro legado `/ensaio.bin`; outros arquivos são preservados.

## Metadados efetivos

| Dado | Origem e persistência |
| --- | --- |
| ID do ensaio | Gerado pelo ESP; guardado no cabeçalho e amostras |
| Nome, data, observações | Texto fornecido no INICIAR/INICIAR_LORA; omitido significa vazio |
| Célula/perfil | Identificações textuais fornecidas pelo computador; não selecionam um perfil dentro do ESP |
| Coeficiente | Copiado do fator que foi explicitamente configurado e validado para a aquisição |
| Referência de tara | Valor interno realmente aplicado por set_offset, copiado no início; somente leitura |
| Unidade | Sempre kgf |
| Sequência e tempo | Gravados em cada amostra, desde índice 1 e tempo 0 |
| Duração e contagem no encerramento | Guardadas no rodapé, quando ele foi escrito e verificado |
| Condição | Derivada da versão, cabeçalho, rodapé e estado operacional; não assume sucesso na ausência do rodapé |

O ESP não possui calendário configurado nesta etapa: `data` é a data declarada pelo computador, não um timestamp de RTC. Não há seleção de unidade, recepção de offset nem edição de metadados depois que o arquivo foi criado. Alterar a calibração atual ou fazer outra tara depois do ensaio não altera seus dados gravados.

## Estrutura física

Formato interno little-endian do ESP32; tipos de tamanho fixo, double IEEE-754, com static_assert para tamanhos e posições dos CRCs. Não é um arquivo CSV. Na V0.17, BT/1 transporta esses bytes sem conversão; o receptor precisa interpretar explicitamente o layout abaixo e o detalhamento em PROTOCOLO_LORA.md, sem depender de structs nativas do seu computador.

| Registro | Bytes | Conteúdo |
| --- | --- | --- |
| Cabeçalho | 392 | Magic 0x42454E33, versão 3, tamanhos dos registros, ID, coeficiente, tara interna, kgf e metadados; reservado e CRC32 |
| Amostra | 64 | Payload de 56 bytes: tempo uint32, índice uint32, empuxo double, coeficiente double, ID de 32 bytes; reservado uint32 e CRC32 |
| Rodapé | 56 | Magic 0x46494D33, resultado, contagem, duração, motivo, tamanho de amostra e CRC32 |

CRC32 usa o polinômio refletido 0xEDB88320, inicialização e XOR final 0xffffffff. Cada CRC cobre os bytes anteriores ao próprio campo no seu registro; campos reservados são inicializados em zero. O rodapé usa resultado 1 para encerramento normal e 2 para falha/parada antes da primeira medição. Cabeçalho, rodapé e identidade são verificados ao consultar; CRC e identidade de cada amostra são verificados ao recuperar blocos ou transmiti-la pelo LoRa.

A criação escreve, faz flush e relê o cabeçalho antes de aceitar a preparação. Durante aquisição, há flush aproximadamente a cada 1000 ms, condicionado ao avanço do loop e à chegada de amostras. No encerramento, escreve rodapé, faz flush, fecha, reabre e confere a estrutura. Arquivo com amostra parcial não recebe um marcador que finja fechamento normal.

`File.flush()`/`close()` não devolvem sucesso/erro no core consultado. A releitura detecta falhas observáveis, mas não promete imunidade a queda de alimentação. CRC detecta corrupção com probabilidade muito alta; não corrige os dados nem substitui validação física da memória.

## Condições apresentadas no catálogo

| condicao | Significado |
| --- | --- |
| em_andamento | Arquivo da aquisição atual, inclusive preparação; protegido |
| concluido | Cabeçalho e rodapé válidos, tamanho/contagem coerentes e encerramento normal |
| incompleto | Falha registrada, ausência/corrupção de encerramento ou cabeçalho inválido |
| legado | `/ensaio.bin` de versão anterior; não tratado como formato 3 |
| nao_suportado | Arquivo identificado com outra versão |
| indisponivel | Entrada encontrada, mas arquivo não pôde ser aberto |

`concluido` confirma a estrutura e o encerramento, não uma varredura integral antecipada dos CRCs de todas as amostras. O campo `integridade_amostras: verificada_por_bloco` torna essa distinção explícita. LER_AMOSTRAS informa os erros por índice, mesmo que o rodapé indique encerramento normal.

A consulta fornece:

- `amostras_disponiveis`: quantidade de slots completos; não inclui bytes finais de amostra/rodapé parcial. Cada slot será validado quando lido.
- `amostras_confirmadas_no_fim`: quantidade do rodapé válido; null sem rodapé confirmado.
- `duracao_ms`: duração registrada no encerramento; null quando esse encerramento não é recuperável. No ensaio ativo, duração corrente em RAM.
- `duracao_observada_ms`: tempo da última amostra completa com CRC/identidade válidos, quando disponível. Não é o instante exato de uma falha de energia.
- `fechamento_confirmado`: existência de rodapé válido, que pode registrar resultado normal ou falha.
- `diagnostico`/`motivo`: fechamento normal, falha registrada, falta de encerramento ou problema de formato/cabeçalho.

O resumo de execução de STATUS usa os estados operacionais CONCLUIDO/FALHOU/EXCLUIDO para o último ensaio desta sessão. O catálogo usa as condições acima para representar arquivos persistentes. Após reset, STATUS não escolhe um arquivo antigo; use LISTAR_ENSAIOS e CONSULTAR_ENSAIO.

## Reinicialização e recuperação parcial

Não existe retomada automática de aquisição, transmissão ou exclusão total após reset. O cache de pedidos e a calibração operacional são voláteis; os arquivos permanecem. É possível listar/consultar/recuperar sem nova calibração. Somente uma nova aquisição exige coeficiente configurado e tara pronta novamente.

Um ensaio interrompido sem rodapé permanece incompleto. Se seu cabeçalho é íntegro, o firmware calcula quantos registros completos cabem nos bytes disponíveis e permite lê-los em blocos. Cada registro valida CRC, índice, identidade, coeficiente e empuxo finito. Registros inválidos não recebem valores estimados; o cliente pode continuar para os seguintes. Se o cabeçalho não for confiável, a recuperação automática é recusada e o arquivo é preservado.

Um rodapé parcial tem menos de 56 bytes, e a amostra tem 64; portanto esse rodapé não é contado como outra amostra. Um registro interrompido com rodapé válido de falha preserva duração e contagem confirmadas, mas permanece classificado como incompleto e não entra no envio LoRa normal. A recuperação USB em blocos continua disponível.

## Memória cheia e exclusões

Antes de criar outro arquivo e antes de gravar uma amostra, o firmware consulta o espaço e mantém margem de **8192 bytes**, além dos registros necessários. Essa reserva é uma medida conservadora para o LittleFS; seu overhead real depende da partição e dos arquivos. Mesmo com espaço estimado, abertura/escrita podem falhar e têm tratamento próprio. A biblioteca de filesystem executa chamadas síncronas; a latência real e a taxa efetiva devem ser medidas na placa.

Se falta espaço para começar, o pedido é rejeitado sem apagar registros. Se acaba durante aquisição, o ensaio encerra como falho/incompleto, tentando preservar um rodapé de falha. A amostra incompleta não é publicada como válida. Arquivos anteriores permanecem.

Exclusões são sempre explícitas e têm resultado por identificador. Não são transacionais: se o reset ocorrer no meio, os arquivos já excluídos continuam excluídos e os demais permanecem. O firmware não retoma a exclusão por conta própria. Após redescobrir a sessão, liste e decida novos pedidos. Arquivos protegidos são pulados e relatados; não há exclusão adiada oculta.

## Registros antigos

`/ensaio.bin` é exposto como LEGADO_ENSAIO_BIN. Prefixo antigo 0x42454E32/version 2 é reconhecido como legado formato 2; um arquivo anterior sem esse cabeçalho é legado sem versão. Não se deduz quantidade/unidade/metadados por divisão de tamanho nem se mistura layout antigo com novo.

LER_AMOSTRAS e ENVIAR recusam esses registros. Eles não são renomeados, regravados ou apagados no boot. Sua remoção exige EXCLUIR_ENSAIOS com o ID legado ou EXCLUIR_TODOS confirmado. Novos ensaios ficam em outro diretório. Formatos futuros/ilegíveis também permanecem preservados até decisão explícita.
