#pragma once
#include <cstdint>
inline uint32_t esp_random(){static uint32_t v=0x12345678;return ++v;}
