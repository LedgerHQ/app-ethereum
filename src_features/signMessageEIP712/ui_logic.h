#ifndef UI_LOGIC_712_H_
#define UI_LOGIC_712_H_

#include <stdint.h>

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
}   t_ui_context;

bool ui_712_init(void);
void ui_712_next_field(void);
void ui_712_new_root_struct(const void *const struct_ptr);
void ui_712_new_field(const void *const field_ptr, const uint8_t *const data, uint8_t length);
void ui_712_end_sign(void);

#endif // UI_LOGIC_712_H_
