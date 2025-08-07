#pragma once

#include <stdint.h>

#ifdef HAVE_MEMORY_PROFILING
#define MP_FILE __FILE__
#define MP_LINE __LINE__
#else
#define MP_FILE NULL
#define MP_LINE 0
#endif
#define mem_buffer_allocate(ptr, size)   mem_buffer_allocate_impl(ptr, size, MP_FILE, MP_LINE)
#define mem_buffer_cleanup(ptr)          mem_buffer_cleanup_impl(ptr, MP_FILE, MP_LINE)
#define mem_alloc_and_format_uint(value) mem_alloc_and_format_uint_impl(value, MP_FILE, MP_LINE)
#define app_mem_strdup(ptr)              app_mem_strdup_impl(ptr, MP_FILE, MP_LINE)

const char *mem_alloc_and_format_uint_impl(uint32_t value, const char *file, int line);
char *app_mem_strdup_impl(const char *s, const char *file, int line);

bool mem_buffer_allocate_impl(void **buffer, uint16_t size, const char *file, int line);
void mem_buffer_cleanup_impl(void **buffer, const char *file, int line);
