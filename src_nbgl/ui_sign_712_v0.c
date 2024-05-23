#include <string.h>    // explicit_bzero
#include "os_utils.h"  // ARRAYLEN
#include "common_ui.h"
#include "ui_nbgl.h"
#include "common_712.h"
#include "ui_message_signing.h"
#include "ui_signing.h"

static nbgl_contentTagValue_t pairs[2];
static nbgl_contentTagValueList_t pairsList;

static void message_review_choice(bool confirm) {
    if (confirm) {
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_message_712_approved);
    } else {
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_message_712_rejected);
    }
}

static char *format_hash(const uint8_t *hash, char *buffer, size_t buffer_size, size_t offset) {
    array_bytes_string(buffer + offset, buffer_size - offset, hash, KECCAK256_HASH_BYTESIZE);
    return buffer + offset;
}

void ui_sign_712_v0(void) {
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

    pairsList.nbPairs = ARRAYLEN(pairs);
    pairsList.pairs = pairs;
    pairsList.nbMaxLinesForValue = 0;

    nbgl_useCaseReview(TYPE_MESSAGE,
                       &pairsList,
                       &C_Review_64px,
                       TEXT_REVIEW_EIP712,
                       NULL,
                       TEXT_SIGN_EIP712,
                       message_review_choice);
}
