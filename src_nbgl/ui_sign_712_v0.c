#include "common_ui.h"
#include "ui_nbgl.h"
#include "common_712.h"
#include "network.h"
#include "ui_message_signing.h"
#include "ui_signing.h"
#include "uint_common.h"

static nbgl_contentTagValue_t pairs[2];
static nbgl_contentTagValueList_t pairsList;

static void messageReviewChoice_cb(bool confirm) {
    if (confirm) {
        nbgl_useCaseStatus("MESSAGE\nSIGNED", true, ui_message_712_approved);
    } else {
        nbgl_useCaseStatus("Message signing\ncancelled", false, ui_message_712_rejected);
    }
}

static char *format_hash(const uint8_t *hash, char *buffer, size_t buffer_size, size_t offset) {
    array_bytes_string(buffer + offset, buffer_size - offset, hash, KECCAK256_HASH_BYTESIZE);
    return buffer + offset;
}

static void setTagValuePairs(void) {
    explicit_bzero(pairs, sizeof(pairs));
    explicit_bzero(&pairsList, sizeof(pairsList));

    pairs[0].item = "Domain hash";
    pairs[0].value = format_hash(tmpCtx.messageSigningContext712.domainHash,
                                 strings.tmp.tmp,
                                 sizeof(strings.tmp.tmp),
                                 0);
    pairs[1].item = "Message hash";
    pairs[1].value = format_hash(tmpCtx.messageSigningContext712.messageHash,
                                 strings.tmp.tmp,
                                 sizeof(strings.tmp.tmp),
                                 70);

    pairsList.nbPairs = 2;
    pairsList.pairs = pairs;
    pairsList.smallCaseForValue = false;
    pairsList.nbMaxLinesForValue = NB_MAX_LINES_IN_REVIEW;
    pairsList.wrapping = false;
}

static void more_data_cb(bool confirm) {
    if (confirm) {
        if (g_position != UI_SIGNING_POSITION_SIGN) {
            setTagValuePairs();
            nbgl_useCaseReviewStreamingContinue(&pairsList, more_data_cb);
            // Switch to signature
            g_position = UI_SIGNING_POSITION_SIGN;
        } else {
            // the last page must contain a long press button
            nbgl_useCaseReviewStreamingFinish(TEXT_SIGN_EIP712, messageReviewChoice_cb);
        }
    } else {
        ui_message_712_rejected();
    }
}

void ui_sign_712_v0(void) {
    g_position = UI_SIGNING_POSITION_START;
    nbgl_useCaseReviewStreamingStart(TYPE_MESSAGE,
                                     &C_Review_64px,
                                     TEXT_REVIEW_EIP712,
                                     NULL,
                                     more_data_cb);
}
