#ifndef PATH_H_
#define PATH_H_

#include <stdbool.h>

#define MAX_PATH_DEPTH  16

extern uint8_t  *path_indexes;

bool    init_path(void);

#endif // PATH_H_
