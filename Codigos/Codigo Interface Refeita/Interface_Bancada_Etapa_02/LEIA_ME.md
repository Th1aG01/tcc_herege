# Interface Bancada — Etapa 02

Interface gráfica da bancada de empuxo, feita em Python com Tkinter/ttk.
Esta etapa continua focada na interface. Os controles não se comunicam com o ESP32.

## Como abrir

1. Extraia o ZIP completo.
2. Abra a pasta `Interface_Bancada_Etapa_02`.
3. No Windows, execute `INICIAR.bat`.

Também é possível abrir pelo terminal, dentro dessa pasta:

```bash
python main.py
```

É necessário Python 3 com Tcl/Tk. Não é necessário instalar pacotes pelo pip.
Mantenha os três arquivos Python juntos na mesma pasta.

## O que foi acrescentado

- **Tara / zerar leitura:** botão próprio, separado de Limpar gráfico. Nesta etapa, exibe uma prévia da ação; não altera leituras de sensores.
- **Limpar gráfico:** limpa os elementos visuais da área reservada para o gráfico. Preserva nome, data, observações, cronômetro e estado do teste. A curva de empuxo ainda será implementada.
- **Cronômetro:** começa em zero ao iniciar um teste, congela ao parar e reinicia do zero no próximo teste. Exibe minutos, segundos e décimos.
- **Conexões USB e LoRa:** indicadores independentes, inicialmente desconectados. Ainda não há detecção de dispositivos.
- **Iniciar motor:** borda, texto e fundo com destaque vermelho discreto. Iniciar teste continua azul.

## Confirmação do motor

1. Clique em **Iniciar motor**.
2. Marque **Sim, eu quero ligar o motor**.
3. Clique em **Confirmar início**.

O botão final fica desabilitado enquanto a caixa estiver desmarcada. Desmarcá-la bloqueia o botão novamente. Cancelar, pressionar Esc ou fechar a janela encerra a confirmação. Cada abertura começa com a caixa desmarcada. A confirmação concluída apenas exibe uma mensagem de demonstração; nenhum comando é enviado.

## Recursos mantidos

- Tema escuro inicial e alternância para o tema claro.
- Janela redimensionável, com controles laterais roláveis.
- Nome do teste, seleção de data por calendário e observações.
- Botões de calibração, iniciar/parar teste, salvar e transmitir por LoRa.

Calibração, tara, salvar e transmitir continuam como prévias. Os dados digitados permanecem somente durante a sessão. A área do gráfico ainda não recebe aquisições.

## Organização dos arquivos

| Arquivo | Responsabilidade |
| --- | --- |
| `main.py` | Layout, temas, controles, cronômetro e indicadores |
| `calendario.py` | Seleção de data |
| `confirmacao_motor.py` | Janela de confirmação e bloqueio pelo checkbox |
| `INICIAR.bat` | Inicialização no Windows |
| `VERSAO.txt` | Identificação desta entrega |

Para ajustes visuais, as cores ficam em `TEMAS`, no início de `main.py`. O método `atualizar_indicador_conexao` muda apenas a apresentação dos estados USB/LoRa; não abre conexões.

## Verificação desta entrega

A interface foi executada e verificada em Linux com Tkinter. Foram conferidos os dois temas, os tamanhos 1100×700, 800×600 e 640×420, o acesso aos controles por rolagem, o cronômetro, a preservação dos campos ao limpar o gráfico e o fluxo de confirmação do motor. O iniciador `.bat` não foi executado em Windows nesta verificação.

## Numeração

A entrega anterior passa a ser considerada **Etapa 01**, conforme combinado, mesmo que documentos antigos usem outra numeração. Esta entrega é **Etapa 02**, em um ZIP separado. A próxima atualização será **Etapa 03**.
