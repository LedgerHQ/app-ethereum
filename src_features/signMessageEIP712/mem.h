#ifndef MEM_H_
#define MEM_H_

#include <stdlib.h>

#define SIZE_MEM_BUFFER 1024

void    mem_init(void);
void    mem_reset(void);
void    *mem_alloc(size_t size);
void    mem_dealloc(size_t size);

#endif // MEM_H_
