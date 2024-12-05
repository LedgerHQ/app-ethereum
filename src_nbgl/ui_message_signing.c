#include "ui_nbgl.h"
#include "ui_logic.h"

void ui_typed_message_review_choice(bool confirm) {
    if (confirm) {
        ui_712_approve();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_idle);
    } else {
        ui_712_reject();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_idle);
    }
}
