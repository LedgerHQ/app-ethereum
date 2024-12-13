#ifndef UI_LOGIC_712_H_
#define UI_LOGIC_712_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdint.h>
#include "ux.h"
#include "uint256.h"
#include "trusted_name.h"

typedef enum { EIP712_FILTERING_BASIC, EIP712_FILTERING_FULL } e_eip712_filtering_mode;
typedef enum {
    EIP712_FIELD_LATER,
    EIP712_FIELD_INCOMING,
    EIP712_NO_MORE_FIELD
} e_eip712_nfs;  // next field state

bool ui_712_init(void);
void ui_712_deinit(void);
e_eip712_nfs ui_712_next_field(void);
bool ui_712_review_struct(const void *const struct_ptr);
bool ui_712_feed_to_display(const void *field_ptr,
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
bool ui_712_filters_counter_incr(void);
void ui_712_token_join_prepare_addr_check(uint8_t index);
void ui_712_token_join_prepare_amount(uint8_t index, const char *name, uint8_t name_length);
void amount_join_set_token_received(void);
bool ui_712_show_raw_key(const void *field_ptr);
bool ui_712_push_new_filter_path(uint32_t path_crc);
void ui_712_set_discarded_path(const char *path, uint8_t length);
const char *ui_712_get_discarded_path(uint8_t *length);
#ifdef HAVE_TRUSTED_NAME
void ui_712_set_trusted_name_requirements(uint8_t type_count,
                                          const e_name_type *types,
                                          uint8_t source_count,
                                          const e_name_source *sources);
#endif
#ifdef SCREEN_SIZE_WALLET
char *get_ui_pairs_buffer(size_t *size);
#endif

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // UI_LOGIC_712_H_
