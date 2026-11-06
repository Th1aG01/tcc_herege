# Bancada de empuxo — TCC — versão 0.1.0

Primeira entrega: aplicativo local para testar o fluxo de aquisição com dados artificiais enquanto o hardware fica pronto. Não exige ESP32 conectado nem receptor LoRa.

## Executar no Windows

1. Extraia **todo** o ZIP. Não execute os arquivos de dentro do ZIP.
2. Abra a pasta `TCC_Bancada_v0_1` e dê dois cliques em `INICIAR.bat`.
3. Se o ambiente Python não for encontrado, execute `PREPARAR.bat` e depois `INICIAR.bat`. A preparação usa o Python 3.15 instalado, cria `.venv` nesta pasta e instala o Matplotlib; exige internet. O Python deve incluir Tkinter e o comando `py`.
4. Se aparecer erro, mantenha a janela do terminal aberta e envie uma captura com o texto completo.

O iniciador procura primeiro `.venv` na pasta do aplicativo e depois o ambiente `%USERPROFILE%\TCC_Bancada\.venv`. Não precisa abrir o Arduino IDE para esta versão.

Alternativa manual, no Prompt de Comando aberto na pasta extraída:

```bat
py -3.15 -m venv .venv
.venv\Scripts\python.exe -m pip install --only-binary=:all: -r requirements.txt
.venv\Scripts\python.exe main.py
```

## Primeiro ensaio

1. Mantenha duração **12 s**, amplitude **1000 N** e taxa **80 amostras/s**.
2. Informe um nome e, se quiser, observações.
3. Clique em **Iniciar simulação**. A gravação no computador começa nesse momento.
4. Observe força em N e kgf, tempo, contagem de amostras e gráfico.
5. Aguarde o estado **CONCLUÍDO**. São 961 amostras, pois os instantes 0 e 12 s são incluídos.
6. Use **Abrir ensaio salvo** para selecionar o CSV recém-criado. Seu JSON deve continuar na mesma pasta e com o mesmo nome-base.

**Encerrar aquisição** salva o trecho obtido até o clique. Cada novo ensaio cria uma pasta própria. É possível escolher outra pasta antes de começar.

Também há um ensaio pronto em `exemplos/`, que pode ser aberto pelo mesmo botão.

## Arquivos de dados

Por padrão, são gravados em `%USERPROFILE%\TCC_Bancada\ensaios`, dentro de uma subpasta com data e identificador:

- `amostras_SIMULADAS.csv`: amostras, leituras brutas, tempo, força e identificação.
- `amostras_SIMULADAS.json`: nome, observações, parâmetros, calibração fictícia e estado final.

Mantenha o par CSV/JSON junto ao copiar ou compartilhar. No Excel, importe o CSV usando separador ponto e vírgula e ponto como separador decimal. A extensão CSV não é uma planilha XLSX.

A cada lote, aproximadamente a cada 100 ms, o aplicativo solicita a gravação em disco. Uma queda abrupta pode perder dados ainda não gravados; esta versão não garante recuperação de uma última linha parcialmente escrita. Um arquivo sem encerramento confirmado é identificado ao reabrir. A conferência de sequência e conversões detecta algumas inconsistências, mas não é uma assinatura digital nem garantia de autenticidade.

## O que esta versão faz e seus limites

- Gera uma curva determinística de demonstração, com ruído artificial; não é modelo validado de motor.
- Preserva as contagens brutas simuladas de 24 bits e converte usando calibração **fictícia**: offset 100000 e fator 0,01 N/contagem. Nunca usar esses números para medições reais.
- Usa 1 kgf = 9,80665 N. Valores negativos pequenos podem aparecer pelo ruído simulado próximo de zero.
- Exibe pico observado sem filtro. Ainda não calcula impulso ou parâmetros finais de desempenho.
- As opções 10 e 80 amostras/s são taxas da simulação; não configuram o pino RATE do HX711.
- Ainda não lê porta serial, LoRa ou sensores, não grava na flash do ESP32 e não controla dispositivos físicos.
- A gravação no computador desta entrega é diferente da futura gravação na flash embarcada.

## Organização do código

| Arquivo | Responsabilidade |
|---|---|
| `main.py` | Iniciar a aplicação |
| `bancada/gui.py` | Janela, botões e atualização do gráfico |
| `bancada/core.py` | Amostras, configuração e conversão de unidades |
| `bancada/simulator.py` | Fonte de dados artificiais |
| `bancada/storage.py` | Gravação incremental e reabertura validada |
| `tests/test_pipeline.py` | Testes do fluxo de dados |
| `docs/formato_dados.md` | Descrição do formato provisório |

Os tempos das amostras vêm da fonte simulada. Um atraso do desenho da janela não altera a sequência gerada. A fonte está separada para futuramente recebermos dados reais do ESP32 sem misturar comunicação com desenho do gráfico.

## Verificação desta entrega

Os oito testes automatizados do processamento e armazenamento passaram em Linux com Python 3.12.14. Foi verificada a importação da interface com Matplotlib 3.10.8 e Tkinter. A janela gráfica e os iniciadores `.bat` ainda precisam ser validados no seu Windows/Python 3.15; não foram executados nesse ambiente aqui.

Para repetir os testes, na pasta do projeto:

```bat
.venv\Scripts\python.exe -m unittest discover -s tests -v
```

## Próxima etapa

Depois de validar a abertura, a simulação e a reabertura no seu computador: definir o protocolo de aquisição do ESP32, integrar primeiro por USB/serial, validar a calibração real e a taxa do HX711, implementar o registro embarcado e integrar LoRa quando houver receptor compatível. Os detalhes do transporte ainda não estão fechados.
