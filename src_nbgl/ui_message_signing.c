#include "ui_nbgl.h"
#include "ui_logic.h"
#include "common_712.h"

static void ui_typed_message_review_choice_common(bool confirm,
                                                  unsigned int (*approve_func)(),
                                                  unsigned int (*reject_func)()) {
    if (confirm) {
        approve_func();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_idle);
    } else {
        reject_func();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_idle);
    }
}

void ui_typed_message_review_choice_v0(bool confirm) {
    ui_typed_message_review_choice_common(confirm, ui_712_approve_cb, ui_712_reject_cb);
}

#ifdef HAVE_EIP712_FULL_SUPPORT
void ui_typed_message_review_choice(bool confirm) {
    ui_typed_message_review_choice_common(confirm, ui_712_approve, ui_712_reject);
}
#endif
