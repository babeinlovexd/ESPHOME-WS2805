#pragma once
#include <cstdint>
#define PROGMEM
inline uint8_t progmem_read_byte(const uint8_t *value) { return *value; }
