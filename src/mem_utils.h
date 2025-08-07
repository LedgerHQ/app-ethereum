#pragma once

#include <stdint.h>

const char *mem_alloc_and_format_uint(uint32_t value);
char *app_mem_strdup(const char *s);

bool mem_buffer_allocate(void **buffer, uint16_t size);
void mem_buffer_cleanup(void **buffer);
