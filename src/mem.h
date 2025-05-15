#pragma once

#include <stdlib.h>

void mem_init(void);
void mem_reset(void);
void *mem_alloc(size_t size);
void mem_dealloc(size_t size);
void *mem_rev_alloc(size_t size);
void mem_rev_dealloc(size_t size);
