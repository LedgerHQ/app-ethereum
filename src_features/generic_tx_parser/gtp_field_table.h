#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "gtp_field.h"

typedef struct {
    e_param_type type;
    char *key;
    char *value;
} s_field_table_entry;

bool field_table_init(void);
void field_table_cleanup(void);
bool add_to_field_table(e_param_type type, const char *key, const char *value);
size_t field_table_size(void);
const s_field_table_entry *get_from_field_table(int index);
