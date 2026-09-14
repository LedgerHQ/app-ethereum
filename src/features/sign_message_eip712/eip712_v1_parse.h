#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "typed_data.h"

bool v1_set_struct_name(uint8_t length, const uint8_t *name);
bool v1_set_struct_field(uint8_t length, const uint8_t *data);

bool v1_set_root(const char *name);
bool v1_set_array(size_t count);
// Returns the completed leaf on final chunk, in-progress leaf on intermediate chunks, NULL on
// error.
const s_struct_712_value *v1_add_field(const uint8_t *data, size_t length, bool more);
bool v1_is_complete(void);
bool v1_domain_complete(void);
bool v1_field_streaming(void);
bool v1_current_field_uninstantiated_array(void);
e_root_type v1_get_root_type(void);
const s_struct_712_field *v1_get_current_field(void);
uint8_t v1_depth_count(void);
const s_struct_712_field *v1_nth_field(uint8_t n);
uint8_t v1_backup_depth_count(void);
const s_struct_712_field *v1_backup_nth_field(uint8_t n);
bool v1_backup_exists(const char *path, size_t length);
void v1_parse_deinit(void);
