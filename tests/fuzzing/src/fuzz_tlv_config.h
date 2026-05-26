#pragma once

#include <stddef.h>
#include <stdint.h>

size_t fuzz_ethereum_custom_mutator(uint8_t *data, size_t size, size_t max_size, unsigned int seed);
