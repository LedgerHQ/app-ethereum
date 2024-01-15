#include "common_ui.h"
#include "ui_nbgl.h"
#include "common_712.h"
#include "network.h"
#include "ui_message_signing.h"
#include "ui_signing.h"

static nbgl_layoutTagValue_t pairs[2];

static void start_review(void);  // forward declaration

static char *format_hash(const uint8_t *hash, char *buffer, size_t buffer_size, size_t offset) {
    snprintf(buffer + offset, buffer_size - offset, "0x%.*H", KECCAK256_HASH_BYTESIZE, hash);
    return buffer + offset;
}

static bool display_review_page(uint8_t page, nbgl_pageContent_t *content) {
    if (page == 0) {
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

        content->type = TAG_VALUE_LIST;
        content->tagValueList.nbPairs = 2;
        content->tagValueList.pairs = (nbgl_layoutTagValue_t *) pairs;
    } else if (page == 1) {
        g_position = UI_SIGNING_POSITION_SIGN;
        content->type = INFO_LONG_PRESS, content->infoLongPress.icon = get_app_icon(true);
        content->infoLongPress.text = TEXT_SIGN_EIP712;
        content->infoLongPress.longPressText = SIGN_BUTTON;
    } else {
        return false;
    }
    // valid page so return true
    return true;
}

static void display_review(void) {
    uint8_t page;

    switch (g_position) {
        case UI_SIGNING_POSITION_REVIEW:
            page = 0;
            break;
        case UI_SIGNING_POSITION_SIGN:
            page = 1;
            break;
        default:
            return;  // should not happen
    }
    nbgl_useCaseRegularReview(page,
                              2,
                              REJECT_BUTTON,
                              NULL,
                              display_review_page,
                              ui_message_review_choice);
}

static void start_review(void) {
    g_position = UI_SIGNING_POSITION_REVIEW;
    display_review();
}

void ui_sign_712_v0(void) {
    g_position = UI_SIGNING_POSITION_START;
    ui_message_start(TEXT_REVIEW_EIP712,
                     &start_review,
                     &ui_message_712_approved,
                     &ui_message_712_rejected);
}
