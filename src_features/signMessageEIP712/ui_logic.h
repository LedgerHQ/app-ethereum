#ifndef UI_LOGIC_712_H_
#define UI_LOGIC_712_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdint.h>
#include "ux.h"

#define UI_712_FIELD_SHOWN         (1 << 0)
#define UI_712_FIELD_NAME_PROVIDED (1 << 1)

typedef enum { EIP712_FILTERING_BASIC, EIP712_FILTERING_FULL } e_eip712_filtering_mode;
typedef enum {
    EIP712_FIELD_LATER,
    EIP712_FIELD_INCOMING,
    EIP712_NO_MORE_FIELD
} e_eip712_nfs;  // next field state

typedef struct {
    bool shown;
    bool end_reached;
    uint8_t filtering_mode;
    uint8_t filters_to_process;
    uint8_t field_flags;
    uint8_t structs_to_review;
} t_ui_context;

bool ui_712_init(void);
void ui_712_deinit(void);
e_eip712_nfs ui_712_next_field(void);
void ui_712_review_struct(const void *const struct_ptr);
bool ui_712_new_field(const void *const field_ptr, const uint8_t *const data, uint8_t length);
void ui_712_end_sign(void);
unsigned int ui_712_approve();
unsigned int ui_712_reject();
void ui_712_set_title(const char *const str, uint8_t length);
void ui_712_set_value(const char *const str, uint8_t length);
void ui_712_message_hash(void);
void ui_712_redraw_generic_step(void);
void ui_712_flag_field(bool show, bool name_provided);
void ui_712_field_flags_reset(void);
void ui_712_finalize_field(void);
void ui_712_set_filtering_mode(e_eip712_filtering_mode mode);
e_eip712_filtering_mode ui_712_get_filtering_mode(void);
void ui_712_set_filters_count(uint8_t count);
uint8_t ui_712_remaining_filters(void);
void ui_712_queue_struct_to_review(void);
void ui_712_notify_filter_change(void);

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // UI_LOGIC_712_H_
