# Interface Bancada — Etapa 06

Interface em Python/Tkinter com empuxo padronizado em kgf e uma janela de log de erros. O formulário de calibração não tem campo de offset nem seletor de unidade. Importação, exportação e exclusão de perfis foram mantidas. Esta etapa mantém o foco no front-end: os perfis são gravados no computador, sem comunicação com o ESP32.

## Como abrir

1. Extraia o ZIP completo.
2. Abra a pasta `Interface_Bancada_Etapa_06`.
3. No Windows, execute `INICIAR.bat`.

Também é possível executar `python main.py` dentro da pasta do projeto. É necessário Python 3.10 ou superior com Tcl/Tk. Não há pacotes para instalar pelo pip. Mantenha todos os arquivos Python e `dados_demonstracao.json` juntos.

## Criar uma configuração de calibração

1. No painel esquerdo, clique em **Perfis de calibração**.
2. Clique em **Novo perfil**.
3. Preencha o nome e o coeficiente da calibração em kgf.
4. Clique em **Salvar perfil**.

O botão **Calibração** também abre o formulário. Se houver uma edição em andamento, ele retorna aos campos já preenchidos.

| Campo | Uso |
| --- | --- |
| Nome do perfil | Obrigatório e único; identifica a configuração na lista |
| Identificação da célula | Modelo, número de série ou outra identificação física |
| Coeficiente de calibração | Obrigatório, calibrado em kgf; aceita sinal positivo ou negativo, mas não zero |
| Capacidade nominal (kgf) | Opcional; valor positivo para identificar a célula |
| Observações | Informações sobre aquela configuração |

A data de gravação é registrada automaticamente. Os campos numéricos aceitam vírgula ou ponto decimal, sem separador de milhar. Valores inválidos e nomes repetidos são indicados no formulário, preservando os dados já salvos.

Nesta etapa os valores são informados manualmente: a tela não mede uma carga de referência, não calcula o coeficiente e não faz a calibração do sensor. Todos os perfis novos usam kgf automaticamente. O botão Tara / zerar leitura continua separado e será ligado ao firmware na etapa de integração.

## Carregar ou editar um perfil

Na lista, selecione uma linha e use:

- **Carregar perfil:** seleciona os valores para a interface. O nome aparece no painel do ensaio como **Perfil na interface**. Não envia os valores ao ESP32 e não altera os gráficos do histórico.
- **Editar perfil:** abre os valores salvos. **Salvar alterações** atualiza somente aquele perfil, preservando os demais.

Salvar e carregar são ações separadas. Se um perfil carregado for alterado, ele precisa ser carregado novamente. Ao reabrir o programa, os perfis continuam na lista, mas nenhum é carregado automaticamente.

Durante um teste, a troca do perfil fica desabilitada. Também não é possível salvar alterações no perfil carregado enquanto o teste estiver em andamento. Consultar os perfis preserva nome, data e observações do ensaio atual.

Alterações não salvas permanecem no formulário ao mudar de aba. Voltar à lista ou fechar o programa solicita confirmação antes de descartá-las.

O botão **Tara / zerar leitura** continua demonstrativo. A remoção do offset da tela não altera o firmware nem a tara realizada no ESP.

## Importar um perfil de outro computador

1. Abra a aba **Calibração** e clique em **Importar JSON**.
2. Selecione um arquivo `.json` de calibração.
3. Os perfis válidos são adicionados à lista e salvos automaticamente no cadastro local.

Não é necessário copiar o arquivo manualmente para a pasta do programa. A interface lê o conteúdo e o reúne ao arquivo `perfis_calibracao.json`, na pasta de dados indicada abaixo. O arquivo escolhido permanece intacto e não precisa continuar no mesmo lugar após a importação.

São aceitos arquivos exportados pela interface e cópias do cadastro completo das Etapas 04, 05 e 06. Um objeto JSON de perfil também é aceito quando contém `nome` e `coeficiente`; os campos opcionais são `celula`, `capacidade_kgf` e `observacoes`. A unidade ausente é interpretada como kgf. Arquivos anteriores que informam unidade e offset continuam sendo lidos conforme a compatibilidade descrita abaixo. A codificação deve ser UTF-8 e o arquivo deve ter até 5 MB.

Cada importação cria novas cópias: não substitui nem atualiza perfis existentes. Nomes repetidos recebem os sufixos **(importado)**, **(importado 2)** e assim por diante. Coeficientes já calibrados em kgf são preservados. Coeficientes de arquivos antigos marcados em N são convertidos como descrito abaixo. A data de gravação passa a ser a da importação neste computador. Se qualquer perfil do arquivo estiver inválido, nenhum deles é adicionado.

Importar não carrega o perfil no ensaio. Selecione a configuração desejada e use **Carregar perfil** depois de conferir os valores.

## Exportar e excluir

- **Exportar perfil:** selecione uma linha, clique no botão e escolha onde salvar o `.json`. A exportação contém somente o perfil selecionado e pode ser importada em outro computador. Ela também serve como uma cópia de segurança antes de excluir. O cadastro local é protegido contra substituição pela exportação.
- **Excluir perfil:** selecione uma linha e confirme na janela que identifica o perfil pelo nome. **Cancelar** mantém tudo como estava. A exclusão é persistente; para recuperar o perfil, será necessário importá-lo de uma cópia ou cadastrá-lo novamente.

Excluir um perfil carregado também o descarrega da interface. Durante um teste, esse perfil fica protegido contra exclusão; perfis que não estão em uso continuam disponíveis para gerenciamento. Se houver falha ao gravar uma importação ou exclusão, a lista anterior é preservada.

## Unidade fixa e compatibilidade com versões anteriores

O empuxo usa **kgf** em todos os testes, gráficos e tabelas. Os quatro ensaios de demonstração desta entrega já estão em kgf. O leitor de histórico também converte valores antigos explicitamente marcados em N: `kgf = N / 9.80665`.

Para os perfis anteriores marcados em N, a compatibilidade segue a fórmula do firmware enviado: `força = leitura corrigida / coeficiente`. Assim, o coeficiente para kgf é o coeficiente para N multiplicado por `9.80665`. A conversão aparece como aviso no log. O cadastro não é regravado apenas por abrir a interface; antes da primeira gravação de um cadastro convertido, uma cópia exata é criada ao lado dele com o nome `perfis_calibracao_antes_etapa06_<identificador>.json`.

O coeficiente de um perfil já em kgf não é convertido novamente. Os arquivos externos usados na importação permanecem intactos. Valores antigos de offset são mantidos apenas como metadados para preservar a compatibilidade dos arquivos; não aparecem no formulário ou no resumo e não são aplicados ao hardware. Perfis novos guardam esse metadado como `null`.

Ao criar um perfil nesta versão, informe diretamente o coeficiente calibrado em kgf. O programa não realiza uma calibração automática com massa de referência.

## Log de erros

No painel esquerdo da aba **Ensaio**, clique em **Log de erros**, abaixo de **Memória de testes**. Role o painel se necessário. O atalho **Ctrl + L** abre a mesma janela a partir das abas da interface.

A janela exibe data e hora com fuso local, nível (**ERRO** ou **AVISO**), origem, mensagem e detalhes técnicos quando disponíveis. Ela é atualizada enquanto permanece aberta e acompanha os temas claro/escuro. O número ao lado do botão conta os erros e avisos da sessão.

São registrados erros de leitura, importação, exportação e gravação de perfis, avisos de validação, problemas ao carregar os exemplos e exceções não tratadas nos callbacks da interface. Os avisos de conversão de perfis antigos também ficam registrados. Uma sessão sem ocorrências mostra **Nenhum erro registrado nesta sessão**.

- **Copiar log:** copia os registros da sessão para colar em outro programa.
- **Salvar .txt:** salva uma cópia dos registros em um local escolhido.
- **Fechar:** fecha somente a janela; os registros permanecem disponíveis durante a sessão.

Os registros também são gravados automaticamente, a partir da primeira ocorrência, na subpasta `logs` da pasta de dados. Cada execução tem um arquivo próprio `erros_<data>_<hora>_<identificador>.log`. A janela mostra somente a sessão atual; os arquivos das sessões anteriores permanecem na pasta. Se a gravação automática falhar, o log informa a falha e mantém os registros em memória para copiar ou salvar.

Nesta etapa não existe conexão com ESP32, HX711, USB ou LoRa. O log não detecta falhas desses equipamentos por conta própria. Para a futura integração, `app.registrar_erro(origem, mensagem, detalhes="", nivel="ERRO")` aceita registros de outras threads sem operar os widgets do Tk. A janela é atualizada pela thread da interface. A integração deverá encaminhar as falhas do firmware e da comunicação para esse método.

## Onde as configurações ficam salvas

Os perfis ficam fora da pasta da interface, para não dependerem do nome da versão do ZIP:

| Sistema | Arquivo |
| --- | --- |
| Windows | `%LOCALAPPDATA%/BRAVO/BancadaEmpuxo/perfis_calibracao.json` |
| Linux | `$XDG_DATA_HOME/BRAVO/BancadaEmpuxo/perfis_calibracao.json`, ou `~/.local/share/BRAVO/BancadaEmpuxo/perfis_calibracao.json` quando a variável não estiver definida |
| macOS | `~/Library/Application Support/BRAVO/BancadaEmpuxo/perfis_calibracao.json` |

No Windows, pressione **Win + R**, cole `%LOCALAPPDATA%/BRAVO/BancadaEmpuxo` e pressione Enter para localizar a pasta, após salvar ou importar o primeiro perfil. Para transportar uma configuração, use **Exportar perfil**. Para um backup completo, copie o arquivo `perfis_calibracao.json` com o programa fechado; a cópia pode ser importada pela interface.

A pasta e o arquivo são criados automaticamente ao salvar ou importar o primeiro perfil válido. A gravação substitui o arquivo apenas depois de preparar o conteúdo completo. Se houver erro de leitura, o arquivo existente é preservado e o cadastro fica indisponível. Se outra janela modificar o arquivo, reabra a interface antes de continuar a edição.

No Windows, os logs ficam em `%LOCALAPPDATA%/BRAVO/BancadaEmpuxo/logs`.

O ZIP não inclui perfis prontos, logs das verificações ou coeficientes de exemplo para uso real.

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
| `calibracao.py` | Cadastro simplificado em kgf, transferência de perfis e confirmação de exclusão |
| `perfis_calibracao.py` | Validação, compatibilidade, transferência JSON e persistência local dos perfis |
| `unidades.py` | Unidade padrão kgf e conversão de registros antigos |
| `log_erros.py` | Registro em arquivo e janela de consulta, cópia e exportação do log |
| `componentes.py` | Painel com rolagem compartilhado |
| `memoria.py` | Consulta e seleção dos testes anteriores |
| `dados_memoria.py` | Memória de testes de demonstração |
| `dados_demonstracao.json` | Metadados e amostras dos quatro testes fictícios |
| `grafico.py` | Gráfico do histórico |
| `dialogos_memoria.py` | Exclusão e prévia de transmissão |
| `calendario.py` | Seleção da data |
| `confirmacao_motor.py` | Confirmação visual do motor |
| `INICIAR.bat` | Inicialização no Windows |

Para a futura integração, `perfil_carregado` contém a configuração selecionada. Os novos coeficientes são informados em kgf. A compatibilidade de perfis antigos em N considera a fórmula leitura corrigida dividida pelo coeficiente. Não há acesso a Flash, USB, LoRa ou ignição nesta entrega.

## Verificação

A interface foi executada em Linux com Tkinter. Foram verificados a ausência de offset e de seletor de unidade, cadastro/edição/carregamento de perfis, importação/exportação/exclusão, conversão de arquivos antigos sem conversão repetida, cópia de segurança do cadastro original e reabertura em nova execução.

O log foi verificado vazio, com avisos de validação, erro de importação, falha de gravação e exceção de callback com traceback. Também foram verificados atualização por outra thread, cópia para a área de transferência, exportação TXT, cancelamento, fechamento/reabertura e preservação em memória quando a gravação automática falha.

A lista e o editor foram conferidos nos temas claro/escuro em 1100×700, 800×600 e 640×420. O log foi conferido em 800×500 e 520×340. Os gráficos da memória em kgf e a confirmação visual do motor foram mantidos. O iniciador `.bat` e os diálogos nativos do Windows não foram executados nesta verificação.

Os arquivos usados na verificação ficam separados e não acompanham o ZIP.

## Numeração

Entrega atual: **Interface_Bancada_Etapa_06.zip**. A Etapa 05 foi preservada. A próxima atualização será **Etapa 07**.
