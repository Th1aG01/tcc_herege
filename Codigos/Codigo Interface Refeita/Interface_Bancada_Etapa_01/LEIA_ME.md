# Interface da bancada — etapa 02

Painel esquerdo acrescentado à janela base do TCC, mantendo os temas claro e escuro.
O espaço à direita fica disponível para os próximos elementos escolhidos pelo Thiago.

## Como abrir no Windows

1. Extraia a pasta inteira do ZIP, incluindo o novo arquivo `calendario.py`.
2. Abra `INICIAR.bat` com dois cliques.
3. Experimente o calendário, os temas, os botões e o redimensionamento da janela.

Também é possível abrir um terminal nessa pasta e executar:

```text
python main.py
```

É necessário Python 3 com Tkinter/Tcl/Tk. Não há pacotes adicionais para instalar com pip.
Se a mensagem de erro mencionar `tkinter`, ajuste a instalação do Python para incluir Tcl/Tk.

## Elementos desta etapa

| Elemento | Comportamento atual |
| --- | --- |
| Nome do teste | Campo editável. |
| Data do teste | Começa na data local do computador. O botão Calendário abre um seletor em português; também é possível clicar no campo da data. |
| Observações | Campo de várias linhas, com rolagem própria. |
| Calibração | Exibe uma mensagem de prévia. A tela de calibração será definida depois. |
| Iniciar teste / Parar teste | Alternam o estado visual e a disponibilidade desses dois botões. |
| Iniciar motor | Exibe uma mensagem de prévia; não envia comando de ignição. |
| Salvar teste | Exibe uma mensagem de prévia; não grava arquivos. |
| Transmitir por LoRa | Exibe uma mensagem de prévia; não transmite dados. |

**Esta versão é apenas a interface.** Não há conexão com ESP32, portas seriais ou LoRa,
aquisição, acionamento físico, alteração de calibração ou persistência dos campos.
Nome, data escolhida e observações ficam na memória enquanto a janela está aberta.

## Uso em notebook

- A janela pode ser redimensionada, respeitando o mínimo de 640 × 420.
- O painel esquerdo tem rolagem quando os controles não cabem na altura disponível.
- A roda do mouse sobre o painel percorre seus controles; sobre as observações, percorre o texto.
- Tab / Shift+Tab percorrem os controles, trazendo o campo focado para a parte visível do painel.
- O calendário navega entre meses e anos, oferece Hoje e pode ser fechado com Cancelar ou Esc.
- O tema inicial é escuro. A escolha de tema vale enquanto o aplicativo permanece aberto.
- A barra de título segue o comportamento do sistema operacional.

## Código organizado para as próximas etapas

- `main.py`: cores, janela, painel rolável, campos e respostas demonstrativas dos botões.
- `calendario.py`: seletor de data, sem dependência externa.
- `INICIAR.bat`: abertura no Windows e exibição de possíveis erros.

Pontos principais para ajustar juntos em `main.py`:

- `TEMAS`: cores.
- `_criar_painel_esquerdo()`: posição dos campos e botões.
- `_iniciar_teste()` e `_parar_teste()`: estados visuais.
- `_mostrar_previa()`: resposta demonstrativa dos demais comandos.
- `self.area_principal`: espaço livre para os próximos componentes à direita.

## Verificação desta entrega

Foram verificadas a sintaxe e a execução em ambiente gráfico Linux: temas claro e escuro
nas dimensões 1100 × 700, 800 × 600 e 640 × 420; preservação dos campos na troca de tema;
estados de iniciar/parar; seleção de 29/02 em ano bissexto; navegação entre dezembro e
janeiro; cancelamento do calendário e acesso ao último botão pela rolagem.
Também foram inspecionados visualmente o calendário e a mensagem demonstrativa do motor.
O lançador `.bat` deve ser conferido no Windows de uso.

Referências de implementação: [Tkinter](https://docs.python.org/3/library/tkinter.html),
[ttk](https://docs.python.org/3/library/tkinter.ttk.html) e
[calendar](https://docs.python.org/3/library/calendar.html).
