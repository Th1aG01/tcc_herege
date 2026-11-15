# Interface Bancada — Etapa 05

Interface em Python/Tkinter com importação, exportação e exclusão de perfis de calibração. Esta etapa mantém o foco no front-end: os perfis são gravados no computador, sem comunicação com o ESP32.

## Como abrir

1. Extraia o ZIP completo.
2. Abra a pasta `Interface_Bancada_Etapa_05`.
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

## Importar um perfil de outro computador

1. Abra a aba **Calibração** e clique em **Importar JSON**.
2. Selecione um arquivo `.json` de calibração.
3. Os perfis válidos são adicionados à lista e salvos automaticamente no cadastro local.

Não é necessário copiar o arquivo manualmente para a pasta do programa. A interface lê o conteúdo e o reúne ao arquivo `perfis_calibracao.json`, na pasta de dados indicada abaixo. O arquivo escolhido permanece intacto e não precisa continuar no mesmo lugar após a importação.

São aceitos arquivos exportados pela interface e cópias do cadastro completo das Etapas 04 e 05. Um objeto JSON de perfil também é aceito quando contém `nome`, `coeficiente` e `unidade`; os campos opcionais são `celula`, `capacidade_kgf`, `offset` e `observacoes`. A codificação deve ser UTF-8 e o arquivo deve ter até 5 MB.

Cada importação cria novas cópias: não substitui nem atualiza perfis existentes. Nomes repetidos recebem os sufixos **(importado)**, **(importado 2)** e assim por diante. Os valores da calibração são preservados, e a data de gravação passa a ser a da importação neste computador. Se qualquer perfil do arquivo estiver inválido, nenhum deles é adicionado.

Importar não carrega o perfil no ensaio. Selecione a configuração desejada e use **Carregar perfil** depois de conferir os valores.

## Exportar e excluir

- **Exportar perfil:** selecione uma linha, clique no botão e escolha onde salvar o `.json`. A exportação contém somente o perfil selecionado e pode ser importada em outro computador. Ela também serve como uma cópia de segurança antes de excluir. O cadastro local é protegido contra substituição pela exportação.
- **Excluir perfil:** selecione uma linha e confirme na janela que identifica o perfil pelo nome. **Cancelar** mantém tudo como estava. A exclusão é persistente; para recuperar o perfil, será necessário importá-lo de uma cópia ou cadastrá-lo novamente.

Excluir um perfil carregado também o descarrega da interface. Durante um teste, esse perfil fica protegido contra exclusão; perfis que não estão em uso continuam disponíveis para gerenciamento. Se houver falha ao gravar uma importação ou exclusão, a lista anterior é preservada.

## Onde as configurações ficam salvas

Os perfis ficam fora da pasta da interface, para não dependerem do nome da versão do ZIP:

| Sistema | Arquivo |
| --- | --- |
| Windows | `%LOCALAPPDATA%/BRAVO/BancadaEmpuxo/perfis_calibracao.json` |
| Linux | `$XDG_DATA_HOME/BRAVO/BancadaEmpuxo/perfis_calibracao.json`, ou `~/.local/share/BRAVO/BancadaEmpuxo/perfis_calibracao.json` quando a variável não estiver definida |
| macOS | `~/Library/Application Support/BRAVO/BancadaEmpuxo/perfis_calibracao.json` |

No Windows, pressione **Win + R**, cole `%LOCALAPPDATA%/BRAVO/BancadaEmpuxo` e pressione Enter para localizar a pasta, após salvar ou importar o primeiro perfil. Para transportar uma configuração, use **Exportar perfil**. Para um backup completo, copie o arquivo `perfis_calibracao.json` com o programa fechado; a cópia pode ser importada pela interface.

A pasta e o arquivo são criados automaticamente ao salvar ou importar o primeiro perfil válido. A gravação substitui o arquivo apenas depois de preparar o conteúdo completo. Se houver erro de leitura, o arquivo existente é preservado e o cadastro fica indisponível. Se outra janela modificar o arquivo, reabra a interface antes de continuar a edição.

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
| `calibracao.py` | Lista, cadastro, edição, carregamento, importação, exportação e confirmação de exclusão |
| `perfis_calibracao.py` | Modelo, validação, transferência JSON e persistência local dos perfis |
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

A interface foi executada em Linux com Tkinter. Foram verificados cadastro, edição, carregamento, importação pelo seletor de arquivos, exportação, cancelamento e confirmação de exclusão, proteção do perfil em uso, preservação do ensaio atual e reabertura em uma nova execução.

Os testes de dados verificaram importação individual e em lote, nomes repetidos, arquivos inválidos, preservação do arquivo original, proteção do cadastro local, gravação com falha e alterações feitas por outra janela. Os layouts claro/escuro foram verificados em 1100×700, 800×600 e 640×420, incluindo a lista e o editor. Os gráficos da memória e a confirmação visual do motor foram conferidos. O iniciador `.bat` e os diálogos nativos do Windows não foram executados nesta verificação.

Os arquivos usados na verificação ficam separados e não acompanham o ZIP.

## Numeração

Entrega atual: **Interface_Bancada_Etapa_05.zip**. A Etapa 04 foi preservada. A próxima atualização será **Etapa 06**.
