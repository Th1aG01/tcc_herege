# Versão 0.4
Atualize firmware e aplicativo juntos. O sketch permanece na pasta firmware/Bancada_TCC_v0_3_1 para facilitar a continuidade; a mensagem de inicialização identifica 0.4.0.

O botão Apagar TODOS está na janela Arquivos no ESP32 e exige confirmação. Exclui somente arquivos tcc_<identificador>.log; não formata a memória nem remove cópias no PC. Aquisição e transferência impedem a exclusão.

struct payload e MontaPayload centralizam a amostra. USB, memória e telemetria usam JSON. Cada linha do registro é um objeto JSON independente (JSON Lines), incluindo cabeçalho e encerramento. Não é um único array que precise permanecer inteiro na RAM.

Chaves da amostra: v=versão, s=sessão, p=contador de pacote ou null no armazenamento/USB, n=sequência da amostra, t=microssegundos desde início, r=contagens HX711, f=OK/SAT, c=CRC16. Sem calibração, não são publicados valores fictícios em kg/N. O CRC usa a representação canônica TCC2 para compatibilidade do verificador.

Envio do arquivo completo continua em blocos TCCF3, reconstruindo exatamente o registro JSON no receptor. Telemetria ao vivo transmite diretamente o JSON da última amostra. O receptor BRAVO textual pode permanecer igual; atualize o aplicativo Python do notebook receptor. O código Luke usa binário no envio ao vivo; adotamos somente a organização da estrutura, não seu protocolo binário nem funções de voo.

Execução no VS Code: abra a pasta contendo main.py, crie o ambiente com py -3 -m venv .venv, instale requirements.txt com .venv/Scripts/python.exe -m pip install --only-binary=:all: -r requirements.txt e execute .venv/Scripts/python.exe main.py.
