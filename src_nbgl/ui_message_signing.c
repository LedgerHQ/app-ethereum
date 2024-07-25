#include "ui_nbgl.h"
#include "ui_logic.h"

static void ui_message_712_approved(void) {
    ui_712_approve();
}

static void ui_message_712_rejected(void) {
    ui_712_reject();
}

void ui_typed_message_review_choice(bool confirm) {
    if (confirm) {
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_message_712_approved);
    } else {
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_message_712_rejected);
    }
}
