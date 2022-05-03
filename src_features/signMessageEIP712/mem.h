#ifndef MEM_H_
#define MEM_H_

#include <stdlib.h>

void    mem_init(void);
void    mem_reset(void);
void    *mem_alloc(size_t size);
void    mem_dealloc(size_t size);

#ifdef DEBUG
extern size_t mem_max;
#endif

#endif // MEM_H_
