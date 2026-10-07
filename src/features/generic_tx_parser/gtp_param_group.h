#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "buffer.h"
#include "lists.h"

// Forward declaration to break circular dependency with gtp_field.h
struct s_field;

typedef enum {
    GROUP_ITER_BUNDLED = 0,
    GROUP_ITER_SEQUENTIAL = 1,
    GROUP_ITER_MAX,
} e_group_iteration_type;

// Linked-list node holding one heap-allocated sub-field
typedef struct s_group_field_node {
    flist_node_t node;
    struct s_field *field;
} s_group_field_node;

typedef struct {
    uint8_t version;
    e_group_iteration_type iteration_type;
    s_group_field_node *fields;
} s_param_group;

typedef struct {
    s_param_group *param;
} s_param_group_context;

bool handle_param_group_struct(const buffer_t *buf, s_param_group_context *context);
bool format_param_group(const struct s_field *field, uint8_t depth);
void cleanup_param_group(s_param_group *group);
