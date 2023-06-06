#ifndef UI_SIGNING_H_
#define UI_SIGNING_H_

typedef enum {
    UI_SIGNING_POSITION_START = 0,
    UI_SIGNING_POSITION_REVIEW,
    UI_SIGNING_POSITION_SIGN
} e_ui_signing_position;

extern e_ui_signing_position g_position;

#endif  // UI_SIGNING_H_
