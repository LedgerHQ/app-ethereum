#ifndef UI_LOGIC_712_H_
#define UI_LOGIC_712_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdint.h>
#include "ux.h"

#define UI_712_FIELD_SHOWN          (1 << 0)
#define UI_712_FIELD_NAME_PROVIDED  (1 << 1)

typedef enum
{
    EIP712_FILTERING_BASIC,
    EIP712_FILTERING_FULL
}   e_eip712_filtering_mode;

typedef enum
{
    UI_712_POS_REVIEW,
    UI_712_POS_END
}   e_ui_position;

typedef struct
{
    bool shown;
    bool end_reached;
    e_ui_position pos;
    uint8_t filtering_mode;
    uint8_t field_flags;
}   t_ui_context;

extern t_ui_context *ui_ctx;

bool ui_712_init(void);
void ui_712_deinit(void);
void ui_712_next_field(void);
void ui_712_new_root_struct(const void *const struct_ptr);
void ui_712_new_field(const void *const field_ptr, const uint8_t *const data, uint8_t length);
void ui_712_end_sign(void);
unsigned int ui_712_approve(const bagl_element_t *e);
unsigned int ui_712_reject(const bagl_element_t *e);
void ui_712_set_title(const char *const str, uint8_t length);
void ui_712_set_value(const char *const str, uint8_t length);
#ifdef HAVE_EIP712_HALF_BLIND
void    ui_712_message_hash(void);
#endif // HAVE_EIP712_HALF_BLIND
void    ui_712_redraw_generic_step(void);
void    ui_712_flag_field(bool show, bool name_provided);
void    ui_712_field_flags_reset(void);

#endif // HAVE_EIP712_FULL_SUPPORT

#endif // UI_LOGIC_712_H_
