#include "ui_nbgl.h"
#include "ui_logic.h"
#include "common_712.h"
#include "common_ui.h"
#include "ui_utils.h"
#include "trusted_name.h"
#include "proxy_info.h"

static void ui_typed_message_review_choice_common(bool confirm,
                                                  nbgl_callback_t approve_func,
                                                  nbgl_callback_t reject_func) {
    if (confirm) {
        approve_func();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_idle);
    } else {
        reject_func();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_idle);
    }
    ui_all_cleanup();
    trusted_name_cleanup();
    proxy_cleanup();
}

void ui_typed_message_review_choice_v0(bool confirm) {
    ui_typed_message_review_choice_common(confirm, ui_712_approve_cb, ui_712_reject_cb);
}

void ui_typed_message_review_choice(bool confirm) {
    ui_typed_message_review_choice_common(confirm, ui_712_approve, ui_712_reject);
}
