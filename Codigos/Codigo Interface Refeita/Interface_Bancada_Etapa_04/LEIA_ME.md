# Interface Bancada — Etapa 04

Interface em Python/Tkinter com perfis de calibração para várias células de carga. Esta etapa mantém o foco no front-end: os perfis são gravados no computador, sem comunicação com o ESP32.

## Como abrir

1. Extraia o ZIP completo.
2. Abra a pasta `Interface_Bancada_Etapa_04`.
3. No Windows, execute `INICIAR.bat`.

Também é possível executar `python main.py` dentro da pasta do projeto. É necessário Python 3.10 ou superior com Tcl/Tk. Não há pacotes para instalar pelo pip. Mantenha todos os arquivos Python e `dados_demonstracao.json` juntos.

## Criar uma configuração de calibração

1. No painel esquerdo, clique em **Perfis de calibração**.
2. Clique em **Novo perfil**.
3. Preencha o nome, os valores da calibração e a unidade.
4. Clique em **Salvar perfil**.

O botão **Calibração** também abre o formulário. Se houver uma edição em andamento, ele retorna aos campos já preenchidos.

| Campo | Uso |
| --- | --- |
| Nome do perfil | Obrigatório e único; identifica a configuração na lista |
| Identificação da célula | Modelo, número de série ou outra identificação física |
| Coeficiente de calibração | Obrigatório; aceita sinal positivo ou negativo, mas não zero |
| Offset — contagem bruta | Opcional; número inteiro, sem presumir zero quando fica em branco |
| Capacidade nominal (kgf) | Opcional; valor positivo para identificar a célula |
| Unidade calibrada | N ou kgf, correspondente ao coeficiente informado |
| Observações | Informações sobre aquela configuração |

A data de gravação é registrada automaticamente. Os campos numéricos aceitam vírgula ou ponto decimal, sem separador de milhar. Valores inválidos e nomes repetidos são indicados no formulário, preservando os dados já salvos.

Nesta etapa os valores são informados manualmente: a tela não mede uma carga de referência, não calcula o coeficiente e não faz a calibração do sensor. A unidade é guardada como parte da configuração; selecionar N ou kgf não converte automaticamente o coeficiente.

## Carregar ou editar um perfil

Na lista, selecione uma linha e use:

- **Carregar perfil:** seleciona os valores para a interface. O nome aparece no painel do ensaio como **Perfil na interface**. Não envia os valores ao ESP32 e não altera os gráficos do histórico.
- **Editar perfil:** abre os valores salvos. **Salvar alterações** atualiza somente aquele perfil, preservando os demais.

Salvar e carregar são ações separadas. Se um perfil carregado for alterado, ele precisa ser carregado novamente. Ao reabrir o programa, os perfis continuam na lista, mas nenhum é carregado automaticamente.

Durante um teste, a troca do perfil fica desabilitada. Também não é possível salvar alterações no perfil carregado enquanto o teste estiver em andamento. Consultar os perfis preserva nome, data e observações do ensaio atual.

Alterações não salvas permanecem no formulário ao mudar de aba. Voltar à lista ou fechar o programa solicita confirmação antes de descartá-las.

O botão **Tara / zerar leitura** continua demonstrativo e não regrava o offset de um perfil.

## Onde as configurações ficam salvas

Os perfis ficam fora da pasta da interface, para não dependerem do nome da versão do ZIP:

| Sistema | Arquivo |
| --- | --- |
| Windows | `%LOCALAPPDATA%/BRAVO/BancadaEmpuxo/perfis_calibracao.json` |
| Linux | `$XDG_DATA_HOME/BRAVO/BancadaEmpuxo/perfis_calibracao.json`, ou `~/.local/share/BRAVO/BancadaEmpuxo/perfis_calibracao.json` quando a variável não estiver definida |
| macOS | `~/Library/Application Support/BRAVO/BancadaEmpuxo/perfis_calibracao.json` |

No Windows, pressione **Win + R**, cole `%LOCALAPPDATA%/BRAVO/BancadaEmpuxo` e pressione Enter para localizar a pasta, após salvar o primeiro perfil. Copie o JSON com o programa fechado para fazer um backup ou levar as configurações para outro computador.

O arquivo só é criado ao salvar o primeiro perfil. A gravação substitui o arquivo apenas depois de preparar o conteúdo completo. Se houver erro de leitura, o arquivo existente é preservado e o cadastro fica indisponível. Se outra janela modificar o arquivo, reabra a interface antes de continuar a edição.

O ZIP não inclui perfis prontos ou coeficientes de exemplo para uso real.

## Recursos anteriores mantidos

- Temas claro e escuro e layout ajustável para notebook.
- Nome, calendário e observações do ensaio.
- Iniciar/parar teste e cronômetro.
- Tara e limpeza do gráfico em controles separados.
- Indicadores USB e LoRa inicialmente desconectados.
- Botão Iniciar motor com destaque vermelho e confirmação por checkbox **Sim, eu quero ligar o motor**.
- Memória de testes com seleção individual/total, confirmação de exclusão em lote e consulta de gráfico, amostras e observações.

A memória de testes ainda usa quatro registros fictícios. Excluir esses registros afeta apenas a sessão; eles voltam ao reiniciar. Isso é diferente dos **perfis de calibração**, que agora são gravados de forma persistente no computador.

Salvar teste e transmitir pelo LoRa continuam como prévias. O gráfico do ensaio atual permanece reservado para uma próxima etapa; os gráficos disponíveis são os do histórico de demonstração.

## Organização do código

| Arquivo | Responsabilidade |
| --- | --- |
| `main.py` | Janela, abas, temas e controles do ensaio |
| `calibracao.py` | Lista, cadastro, edição e carregamento de perfis |
| `perfis_calibracao.py` | Modelo, validação e persistência local dos perfis |
| `componentes.py` | Painel com rolagem compartilhado |
| `memoria.py` | Consulta e seleção dos testes anteriores |
| `dados_memoria.py` | Memória de testes de demonstração |
| `dados_demonstracao.json` | Metadados e amostras dos quatro testes fictícios |
| `grafico.py` | Gráfico do histórico |
| `dialogos_memoria.py` | Exclusão e prévia de transmissão |
| `calendario.py` | Seleção da data |
| `confirmacao_motor.py` | Confirmação visual do motor |
| `INICIAR.bat` | Inicialização no Windows |

Para a futura integração, `perfil_carregado` contém a configuração selecionada. Os valores são armazenados sem assumir uma fórmula de conversão específica do firmware. Não há acesso a Flash, USB, LoRa ou ignição nesta entrega.

## Verificação

A interface foi executada em Linux com Tkinter. Foram verificados cadastro, edição, carregamento, preservação do ensaio atual, validação, nomes repetidos, cancelamento de alterações, reabertura em uma nova execução e os layouts claro/escuro em 1100×700, 800×600 e 640×420. Os testes de persistência também verificaram falha de gravação e preservação de arquivo inválido. O iniciador `.bat` não foi executado em Windows nesta verificação.

## Numeração

Entrega atual: **Interface_Bancada_Etapa_04.zip**. A Etapa 03 foi preservada. A próxima atualização será **Etapa 05**.
