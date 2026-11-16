#pragma once
#include <stdint.h>
// Entrega segura: somente leitura. Habilitar apenas apos validar/autorizar a memoria.
#ifndef FLASH_PERMITIR_ESCRITA
#define FLASH_PERMITIR_ESCRITA 0
#endif
constexpr int FLASH_CS = 13;
constexpr int SPI_SCK_BANCADA = 18, SPI_MISO_BANCADA = 19, SPI_MOSI_BANCADA = 23;
constexpr int SPI_CS_RADIO = 21;
constexpr uint32_t FLASH_CAPACIDADE = 2097152;
constexpr uint32_t FLASH_JEDEC_ESPERADO = 0xEF4015;
constexpr uint32_t FLASH_SPI_HZ = 1000000;
