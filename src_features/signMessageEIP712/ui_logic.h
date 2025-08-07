#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "ux.h"
#include "uint256.h"
#include "typed_data.h"
#include "trusted_name.h"
#include "list.h"

typedef enum { EIP712_FILTERING_BASIC, EIP712_FILTERING_FULL } e_eip712_filtering_mode;
typedef enum {
    EIP712_FIELD_LATER,
    EIP712_FIELD_INCOMING,
    EIP712_NO_MORE_FIELD
} e_eip712_nfs;  // next field state

typedef struct ui_712_pair {
    s_flist_node _list;
    char *key;
    char *value;
} s_ui_712_pair;

bool ui_712_init(void);
void ui_712_deinit(void);
e_eip712_nfs ui_712_next_field(void);
bool ui_712_review_struct(const s_struct_712 *struct_ptr);
bool ui_712_feed_to_display(const s_struct_712_field *field_ptr,
                            const uint8_t *data,
                            uint8_t length,
                            bool first,
                            bool last);
void ui_712_end_sign(void);
unsigned int ui_712_approve();
unsigned int ui_712_reject();
void ui_712_set_title(const char *str, size_t length);
void ui_712_set_value(const char *str, size_t length);
void ui_712_message_hash(void);
bool ui_712_redraw_generic_step(void);
void ui_712_flag_field(bool show,
                       bool name_provided,
                       bool token_join,
                       bool datetime,
                       bool contract_name);
void ui_712_field_flags_reset(void);
void ui_712_finalize_field(void);
void ui_712_set_filtering_mode(e_eip712_filtering_mode mode);
e_eip712_filtering_mode ui_712_get_filtering_mode(void);
void ui_712_set_filters_count(uint8_t count);
uint8_t ui_712_remaining_filters(void);
void ui_712_queue_struct_to_review(void);
void ui_712_token_join_prepare_addr_check(uint8_t index);
bool ui_712_token_join_prepare_amount(uint8_t index, const char *name, uint8_t name_length);
bool amount_join_set_token_received(void);
bool ui_712_show_raw_key(const s_struct_712_field *field_ptr);
bool ui_712_push_new_filter_path(uint32_t path_crc);
bool ui_712_set_discarded_path(const char *path, uint8_t length);
const char *ui_712_get_discarded_path(void);
void ui_712_clear_discarded_path(void);
void ui_712_set_trusted_name_requirements(uint8_t type_count,
                                          const e_name_type *types,
                                          uint8_t source_count,
                                          const e_name_source *sources);
const s_ui_712_pair *ui_712_get_pairs(void);
bool ui_712_push_new_pair(const char *key, const char *value);
void ui_712_delete_pairs(size_t keep);
