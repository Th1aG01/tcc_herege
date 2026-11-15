# Interface Bancada — Etapa 03

Interface gráfica em Python com Tkinter/ttk. Esta entrega adiciona a memória de testes e a consulta do histórico, mantendo o foco no front-end.

## Como abrir

1. Extraia o ZIP completo.
2. Abra a pasta `Interface_Bancada_Etapa_03`.
3. No Windows, execute `INICIAR.bat`.

Também é possível executar `python main.py` no terminal, dentro da pasta do projeto. É necessário Python 3 com Tcl/Tk. Não há pacotes para instalar pelo pip.

Mantenha todos os arquivos Python e `dados_demonstracao.json` juntos. O arquivo JSON contém os quatro testes fictícios usados nesta prévia.

## Memória de testes

O botão **Memória de testes**, no painel esquerdo, abre a aba de mesmo nome. Você também pode acessar a aba diretamente no topo da interface.

A lista mostra nome, data e duração dos testes, com um checkbox ao lado de cada registro. **Selecionar todos** marca ou desmarca a lista inteira. O contador informa quantos registros estão marcados.

| Ação | Seleção necessária | Comportamento nesta etapa |
| --- | --- | --- |
| Abrir gráfico | Exatamente um teste | Abre o histórico dentro da aba Memória de testes |
| Transmitir por LoRa | Um ou vários testes | Exibe os registros e a quantidade de amostras que seriam transmitidos; não envia dados |
| Excluir selecionados | Um ou vários testes | Pede confirmação com quantidade e nomes e remove somente os itens marcados da sessão de demonstração |

Cancelar a confirmação, pressionar Esc ou fechar a janela preserva os registros. Quando todos forem removidos, a lista exibe o estado vazio e desabilita as ações que dependem de seleção.

## Consulta de um teste anterior

O histórico mostra nome, data, duração e quantidade de amostras, com três visualizações:

- **Gráfico:** empuxo em função do tempo, usando os valores guardados no registro. Não há regeneração aleatória nem suavização da curva ao abrir o teste.
- **Amostras:** tabela de tempo e empuxo, com três casas decimais.
- **Observações:** texto associado ao registro, somente para leitura.

O botão **Voltar à lista** retorna à memória. O botão **Transmitir por LoRa** do histórico usa apenas o teste aberto.

A aba **Ensaio** continua separada: consultar históricos não substitui seu nome, sua data ou suas observações e não interrompe o cronômetro de um ensaio em andamento.

## Escopo da demonstração

Esta interface ainda não lê a memória Flash do ESP32, não abre conexões USB e não transmite pelo LoRa. Os quatro testes são exemplos fictícios para avaliar os controles e o gráfico.

As exclusões afetam apenas a sessão atual. Ao fechar e abrir o programa, os exemplos retornam, porque `dados_demonstracao.json` é somente lido. Nenhum teste real é apagado. O botão Salvar teste ainda exibe uma prévia e não adiciona ensaios à memória.

## Recursos anteriores mantidos

- Tema escuro inicial, com opção de tema claro.
- Janela redimensionável, painel lateral com rolagem e cabeçalho compacto em janelas baixas.
- Nome, calendário, observações, calibração e tara/zerar leitura.
- Iniciar/parar teste com cronômetro; limpar gráfico sem apagar os campos.
- Indicadores USB e LoRa inicialmente desconectados.
- Iniciar motor com destaque vermelho e confirmação por checkbox **Sim, eu quero ligar o motor**. Nenhum comando de ignição é enviado.

O gráfico do ensaio atual continua reservado para uma etapa posterior; o gráfico implementado nesta entrega é o de consulta dos registros da memória.

## Organização do código

| Arquivo | Responsabilidade |
| --- | --- |
| `main.py` | Janela, abas principais, temas e controles do ensaio |
| `componentes.py` | Painel com rolagem compartilhado |
| `memoria.py` | Lista, seleção, ações e navegação do histórico |
| `dados_memoria.py` | Modelo dos registros e memória de demonstração em sessão |
| `dados_demonstracao.json` | Metadados e amostras dos quatro testes fictícios |
| `grafico.py` | Desenho do gráfico com as amostras do registro |
| `dialogos_memoria.py` | Confirmação de exclusão e prévia de transmissão |
| `calendario.py` | Seleção da data |
| `confirmacao_motor.py` | Confirmação visual do motor |
| `INICIAR.bat` | Inicialização no Windows |

## Verificação

A interface foi executada em Linux com Tkinter. Foram verificados os temas claro e escuro nos tamanhos 1100×700, 800×600 e 640×420, seleção individual e total, exclusão por lote e cancelamento, estado vazio, correspondência entre gráfico/tabela e dados do registro, preservação do ensaio atual e confirmação do motor. O iniciador `.bat` não foi executado em Windows nesta verificação.

## Numeração

Entrega atual: **Interface_Bancada_Etapa_03.zip**. A Etapa 02 foi preservada. A próxima atualização será **Etapa 04**.
