# Validação v0.4.0
44 testes automatizados: controlador, cronômetro, armazenamento, transporte e recuperação de registros. O teste C++ usa o fonte do firmware com periféricos simulados; verifica exclusão em lote preservando config.json, bloqueio durante aquisição e ausência de registros. O JSON emitido pelo C++ é interpretado no Python e um registro JSON é reaberto com integridade e sequência conferidas.
A interface foi verificada por testes de controlador, sem inspeção visual no Windows. Ainda é necessário testar na protoboard e com o receptor físico.

Compilação real aprovada para DOIT ESP32 DEVKIT V1 com core 3.3.12, LoRa 0.8.0 e HX711 0.7.5: 349604 bytes de programa, 23380 bytes de variáveis globais.
