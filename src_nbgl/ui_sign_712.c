#ifdef HAVE_EIP712_FULL_SUPPORT

#include <string.h>  // explicit_bzero
#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_logic.h"
#include "common_712.h"
#include "nbgl_use_case.h"
#include "ui_message_signing.h"

static nbgl_contentTagValue_t pair;
static nbgl_contentTagValueList_t pairs_list;

static void message_progress(bool confirm) {
    if (confirm) {
        if (ui_712_next_field() == EIP712_NO_MORE_FIELD) {
            ui_712_switch_to_sign();
        }
    } else {
        ui_typed_message_review_choice(false);
    }
}

static void message_update(bool confirm) {
    if (confirm) {
        explicit_bzero(&pair, sizeof(pair));
        explicit_bzero(&pairs_list, sizeof(pairs_list));

        pair.item = strings.tmp.tmp2;
        pair.value = strings.tmp.tmp;
        pairs_list.nbPairs = 1;
        pairs_list.pairs = &pair;
        pairs_list.wrapping = false;
        nbgl_useCaseReviewStreamingContinue(&pairs_list, message_progress);
    } else {
        ui_typed_message_review_choice(false);
    }
}

void ui_712_start(void) {
    nbgl_useCaseReviewStreamingStart(TYPE_MESSAGE,
                                     &C_Review_64px,
                                     TEXT_REVIEW_EIP712,
                                     NULL,
                                     message_update);
}

void ui_712_switch_to_message(void) {
    message_update(true);
}

void ui_712_switch_to_sign(void) {
    nbgl_useCaseReviewStreamingFinish(TEXT_SIGN_EIP712, ui_typed_message_review_choice);
}

#endif  // HAVE_EIP712_FULL_SUPPORT
