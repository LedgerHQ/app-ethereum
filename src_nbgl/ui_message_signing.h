#ifndef UI_MESSAGE_SIGNING_H_
#define UI_MESSAGE_SIGNING_H_

#include <stdbool.h>
#include "ui_signing.h"

#define TEXT_MESSAGE           "message"
#define TEXT_TYPED_MESSAGE     "typed " TEXT_MESSAGE
#define TEXT_REVIEW_EIP712     REVIEW(TEXT_TYPED_MESSAGE)
#define TEXT_SIGN_EIP712       SIGN(TEXT_TYPED_MESSAGE)
#define TEXT_BLIND_SIGN_EIP712 BLIND_SIGN(TEXT_TYPED_MESSAGE)

void ui_typed_message_review_choice_v0(bool confirm);
#ifdef HAVE_EIP712_FULL_SUPPORT
void ui_typed_message_review_choice(bool confirm);
#endif

#endif  // UI_MESSAGE_SIGNING_H_
