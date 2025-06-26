#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef HAVE_MEMORY_PROFILING
#define MP_FILE __FILE__
#define MP_LINE __LINE__
#else
#define MP_FILE NULL
#define MP_LINE 0
#endif
#define app_mem_alloc(size) app_mem_alloc_impl(size, MP_FILE, MP_LINE)
#define app_mem_free(ptr)   app_mem_free_impl(ptr, MP_FILE, MP_LINE)

bool app_mem_init(void);
void *app_mem_alloc_impl(size_t size, const char *file, int line);
void app_mem_free_impl(void *ptr, const char *file, int line);
