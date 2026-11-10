# TCC — Bancada de empuxo v0.3.0

**Testar por USB; depois enviar o registro completo por LoRa.**

Comece por **[LEIA_PRIMEIRO.md](LEIA_PRIMEIRO.md)**.

| Onde executar | Arquivo | Função |
|---|---|---|
| Em cada computador que usará o aplicativo | `PREPARAR.bat` | Instalar dependências Python, uma vez |
| Computador conectado à bancada | `INICIAR.bat` | Controlar, visualizar e salvar o teste por USB |
| Notebook conectado ao receptor | `RECEPTOR_LORA.bat` | Receber o registro, salvar CSV e mostrar a curva |
| Opcional, sem hardware | `SIMULACAO.bat` | Simulador anterior |

Grave `firmware/Bancada_TCC_v0_3/Bancada_TCC_v0_3.ino` somente no ESP32 da bancada. O receptor BRAVO foi preservado.

Célula de teste de 5 kg **sem calibração**. Leituras reais em contagens. Não comanda ignição ou atuadores.

O envio posterior requer cópia interna do ensaio no ESP32. A confirmação de recebimento aparece no notebook receptor, após verificar o arquivo inteiro.
