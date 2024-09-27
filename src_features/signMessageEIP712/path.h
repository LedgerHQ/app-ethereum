#ifndef PATH_H_
#define PATH_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdint.h>
#include <stdbool.h>

#define MAX_PATH_DEPTH  16
#define MAX_ARRAY_DEPTH 8

typedef struct {
    uint8_t path_index;
    uint8_t size;
    uint8_t index;
} s_array_depth;

typedef enum { ROOT_DOMAIN, ROOT_MESSAGE } e_root_type;

typedef struct {
    uint8_t depth_count;
    uint8_t depths[MAX_PATH_DEPTH];
    uint8_t array_depth_count;
    s_array_depth array_depths[MAX_ARRAY_DEPTH];
    const void *root_struct;
    e_root_type root_type;
} s_path;

bool path_set_root(const char *const struct_name, uint8_t length);
const void *path_get_field(void);
bool path_advance(bool do_typehash);
bool path_init(void);
void path_deinit(void);
bool path_new_array_depth(const uint8_t *const data, uint8_t length);
e_root_type path_get_root_type(void);
const void *path_get_root(void);
const void *path_get_nth_field(uint8_t n);
const void *path_backup_get_nth_field(uint8_t n);
bool path_exists_in_backup(const char *path, size_t length);
const void *path_get_nth_field_to_last(uint8_t n);
uint8_t path_get_depth_count(void);
uint8_t path_backup_get_depth_count(void);

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // PATH_H_
