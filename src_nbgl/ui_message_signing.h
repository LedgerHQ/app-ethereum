#ifndef UI_MESSAGE_SIGNING_H_
#define UI_MESSAGE_SIGNING_H_

#include <stdbool.h>

#define SIGN(msg)          "Sign " msg "?"
#define BLIND_SIGN(msg)    "Accept risk and sign " msg "?"
#define REVIEW(msg)        "Review " msg
#define TEXT_TRANSACTION   "transaction"
#define TEXT_MESSAGE       "message"
#define TEXT_TYPED_MESSAGE "typed " TEXT_MESSAGE

#define TEXT_REVIEW_TRANSACTION REVIEW(TEXT_TRANSACTION)
#define TEXT_SIGN_TRANSACTION   SIGN(TEXT_TRANSACTION)
#define TEXT_BLIND_TRANSACTION  BLIND_SIGN(TEXT_TRANSACTION)
#define TEXT_REVIEW_EIP191      REVIEW(TEXT_MESSAGE)
#define TEXT_SIGN_EIP191        SIGN(TEXT_MESSAGE)
#define TEXT_REVIEW_EIP712      REVIEW(TEXT_TYPED_MESSAGE)
#define TEXT_SIGN_EIP712        SIGN(TEXT_TYPED_MESSAGE)
#define TEXT_BLIND_SIGN_EIP712  BLIND_SIGN(TEXT_TYPED_MESSAGE)

void ui_typed_message_review_choice_v0(bool confirm);
#ifdef HAVE_EIP712_FULL_SUPPORT
void ui_typed_message_review_choice(bool confirm);
#endif

#endif  // UI_MESSAGE_SIGNING_H_
