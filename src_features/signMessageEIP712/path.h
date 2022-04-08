#ifndef PATH_H_
#define PATH_H_

#include <stdint.h>
#include <stdbool.h>

#define MAX_PATH_DEPTH  16
#define MAX_ARRAY_DEPTH 4

typedef struct __attribute__((packed))
{
    uint8_t path_index;
    uint8_t size;
}   s_array_depth;

typedef struct __attribute__((packed))
{
    const void *root_struct;
    uint8_t depth_count;
    uint8_t depths[MAX_PATH_DEPTH];
    uint8_t array_depth_count;
    s_array_depth array_depths[MAX_ARRAY_DEPTH];
}   s_path;

bool    path_set_root(const char *const struct_name, uint8_t length);
const void  *path_get_field(void);
bool    path_advance(void);
bool    path_init(void);
bool    path_new_array_depth(uint8_t size);

#endif // PATH_H_
