#ifdef HAVE_EIP712_FULL_SUPPORT

#include <string.h>  // explicit_bzero
#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_logic.h"
#include "common_712.h"
#include "nbgl_use_case.h"
#include "ui_message_signing.h"
#include "ledger_assert.h"

static nbgl_contentTagValue_t pairs[6];
static nbgl_contentTagValueList_t pairs_list;
static uint8_t pair_idx;
static size_t buf_idx;

static void message_progress(bool confirm) {
    char *buf;
    size_t buf_size;
    size_t shift_off;

    if (pairs_list.nbPairs < pair_idx) {
        buf = get_ui_pairs_buffer(&buf_size);
        memmove(&pairs[0], &pairs[pairs_list.nbPairs], sizeof(pairs[0]));
        memmove(buf, pairs[0].item, (buf + buf_idx) - pairs[0].item);
        shift_off = pairs[0].item - buf;
        buf_idx -= shift_off;
        pairs[0].value -= shift_off;
        pairs[0].item = buf;
        pair_idx = 1;
    }
    if (confirm) {
        if (ui_712_next_field() == EIP712_NO_MORE_FIELD) {
            ui_712_switch_to_sign();
        }
    } else {
        ui_typed_message_review_choice(false);
    }
}

static void message_update(bool confirm) {
    char *buf;
    size_t buf_size;
    size_t buf_off;
    bool flag;

    buf = get_ui_pairs_buffer(&buf_size);
    if (confirm) {
        buf_off = strlen(strings.tmp.tmp2) + 1;
        LEDGER_ASSERT((buf_idx + buf_off) < buf_size, "UI pairs buffer overflow");
        pairs[pair_idx].item = memmove(buf + buf_idx, strings.tmp.tmp2, buf_off);
        buf_idx += buf_off;
        buf_off = strlen(strings.tmp.tmp) + 1;
        LEDGER_ASSERT((buf_idx + buf_off) < buf_size, "UI pairs buffer overflow");
        pairs[pair_idx].value = memmove(buf + buf_idx, strings.tmp.tmp, buf_off);
        buf_idx += buf_off;
        pair_idx += 1;
        pairs_list.nbPairs = nbgl_useCaseGetNbTagValuesInPage(pair_idx, &pairs_list, 0, &flag);
        if (pairs_list.nbPairs < pair_idx) {
            nbgl_useCaseReviewStreamingContinue(&pairs_list, message_progress);
        } else {
            message_progress(true);
        }
    } else {
        ui_typed_message_review_choice(false);
    }
}

void ui_712_start(void) {
    explicit_bzero(&pairs, sizeof(pairs));
    explicit_bzero(&pairs_list, sizeof(pairs_list));
    pairs_list.pairs = pairs;
    pair_idx = 0;
    buf_idx = 0;

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
    if (pair_idx > 0) {
        pairs_list.nbPairs = pair_idx;
        pair_idx = 0;
        nbgl_useCaseReviewStreamingContinue(&pairs_list, message_progress);
    } else {
        nbgl_useCaseReviewStreamingFinish(TEXT_SIGN_EIP712, ui_typed_message_review_choice);
    }
}

#endif  // HAVE_EIP712_FULL_SUPPORT
