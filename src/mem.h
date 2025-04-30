#ifndef MEM_H_
#define MEM_H_

#ifdef HAVE_DYN_MEM_ALLOC

#include <stdlib.h>

void mem_legacy_init(void);
void mem_legacy_reset(void);
void *mem_legacy_alloc(size_t size);
void mem_legacy_dealloc(size_t size);
void *mem_legacy_rev_alloc(size_t size);
void mem_legacy_rev_dealloc(size_t size);

#endif  // HAVE_DYN_MEM_ALLOC

#endif  // MEM_H_
