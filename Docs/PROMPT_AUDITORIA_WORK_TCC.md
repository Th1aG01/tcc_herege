# Prompt — Auditoria integral da bancada de empuxo (somente leitura)

Estamos desenvolvendo o TCC da bancada de empuxo da equipe BRAVO/Univap. Precisamos verificar a versão atual antes de iniciar a integração e atualizar a documentação. Leia o checkpoint anexo `CHECKPOINT_TCC_2026-11-15.md` para o contexto, mas **trate os arquivos reais como fonte principal**, identificando quaisquer divergências.

## Material de entrada

- Firmware ESP32 **V0.17**, integral, inclusive `.ino`, `.h`, testes e `TESTES_LORA.md`.
- Aplicativo Python atual, com todos os módulos, assets e dependências.
- Arquivos de protocolo e esquema/pinagem atual, se disponíveis.
- Histórico de testes e commits Git, se acessíveis.

## Regras

1. **Não alterar arquivos e não refatorar durante esta auditoria.** Produzir diagnóstico e plano, não uma implementação nova.
2. Verificar versão e listar todos os arquivos/módulos relevantes. Citar arquivo e função/linhas para cada achado importante.
3. Diferenciar resultados de inspeção, execução de teste, compilação e testes físicos; nunca declarar funcionamento do hardware sem evidência.
4. Não modificar configurações atuais de rádio, Flash, pinos, unidade kgf nem protocolos sem reportar o motivo.
5. Priorizar segurança e integridade de dados. Não executar ignição física nem acionar cargas energéticas; identificar bloqueios/intertravamentos necessários.

## Revisão requisitada

- **Arquitetura:** mapa ESP32/GUI/receptor, separação de responsabilidades, duplicações, funções supérfluas, módulos muito acoplados, regiões bloqueantes e potenciais pontos de falha.
- **Protocolo USB/Serial:** velocidade/formatos JSON, comandos e respostas, estados, timeouts, unidades, mensagens de erro, compatibilidade exata entre ESP32 e Python.
- **Aquisição:** HX711, calibração e tara, kgf, tempo/índices das amostras, buffers, perda de amostras, duração e interrupções.
- **Flash:** modelo esperado, estrutura de registros, identificação de ensaios, integridade, recuperação após reinicialização, leitura, exclusão e não sobrescrita silenciosa.
- **LoRa:** configurações preservadas, operação ao vivo vs recuperação, identificação de blocos, CRC, confirmação, retransmissões, progresso, cancelamento, limites de tamanho e tratamento correto do LED amarelo.
- **GUI:** cada botão/entrada/indicador, comportamento real e compatibilidade com os comandos do ESP32. Atenção ao botão de ignição e seus controles de segurança locais.
- **Documentação:** diferenças entre a implementação atual e o texto TG2 anterior, inclusive opções técnicas abandonadas.
- **Qualidade:** organização por módulos, legibilidade, código morto, duplicações, recuperação de erro, logs, dependências, testabilidade.

## Saída exigida

1. Inventário de arquivos com versão e papel de cada módulo.
2. Tabela de funcionalidades `implementado / parcialmente implementado / ausente / não verificável`.
3. Matriz **comando da GUI ↔ comando do ESP ↔ formato ↔ resposta ↔ erros/estado**.
4. Problemas classificados **P0 bloqueante/segurança, P1 integração, P2 confiabilidade, P3 acabamento**, com arquivo, linha/função, motivo, risco, solução mínima e teste correspondente.
5. Lista do que **não precisa ser modificado**, para evitar retrabalho.
6. Testes executados com comandos, ambiente e resultado; testes não executados e motivo.
7. Proposta de divisão de tarefas futuras em etapas pequenas, cada uma com arquivos afetados e critério de conclusão.
8. Avaliação fundamentada da necessidade (ou não) de modularizar o `.ino`, mantendo comportamento e pinos.
9. Confronto específico com o `CHECKPOINT_TCC_2026-11-15.md`, corrigindo hipóteses indevidas.

**Não entregue uma nova versão do firmware nesta rodada.** O resultado é um relatório de auditoria para discussão no Chat de coordenação. A próxima implementação só começa depois de definirmos a prioridade dos achados.
