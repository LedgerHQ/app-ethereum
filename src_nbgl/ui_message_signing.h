#ifndef UI_MESSAGE_SIGNING_H_
#define UI_MESSAGE_SIGNING_H_

#include <stdbool.h>

void ui_typed_message_review_choice_v0(bool confirm);
#ifdef HAVE_EIP712_FULL_SUPPORT
void ui_typed_message_review_choice(bool confirm);
#endif

#endif  // UI_MESSAGE_SIGNING_H_
