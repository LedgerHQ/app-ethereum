#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_logic.h"
#include "common_712.h"
#include "nbgl_use_case.h"
#include "network.h"

// 4 pairs of tag/value to display
static nbgl_layoutTagValue_t tlv;

static void reject_message(void) {
    ui_712_reject(NULL);
}

static void sign_message() {
    ui_712_approve(NULL);
}

static void reviewChoice(bool confirm) {
    if (confirm) {
        sign_message();
    } else {
        reject_message();
    }
}
static bool displaySignPage(uint8_t page, nbgl_pageContent_t *content) {
    (void) page;
    content->type = INFO_LONG_PRESS, content->infoLongPress.icon = get_app_icon(true);
    content->infoLongPress.text = "Sign typed message";
    content->infoLongPress.longPressText = "Hold to sign";
    return true;
}

static uint32_t stringsIdx = 0;

static bool displayTransactionPage(uint8_t page, nbgl_pageContent_t *content) {
    uint16_t len = 0;
    if (stringsIdx < strlen(strings.tmp.tmp)) {
        bool reached = nbgl_getTextMaxLenInNbLines(BAGL_FONT_INTER_REGULAR_32px,
                                                   strings.tmp.tmp + stringsIdx,
                                                   SCREEN_WIDTH - (2 * BORDER_MARGIN),
                                                   9,
                                                   &len);
        memset(staxSharedBuffer, 0, sizeof(staxSharedBuffer));
        memcpy(staxSharedBuffer, strings.tmp.tmp + stringsIdx, len);
        stringsIdx += len;
        tlv.item = strings.tmp.tmp2;
        tlv.value = staxSharedBuffer;
        content->type = TAG_VALUE_LIST;
        content->tagValueList.nbPairs = 1;
        content->tagValueList.pairs = &tlv;
        return true;
    } else {
        stringsIdx = 0;
        switch (ui_712_next_field()) {
            case EIP712_NO_MORE_FIELD:
                return displaySignPage(page, content);
                break;
            case EIP712_FIELD_INCOMING:
            case EIP712_FIELD_LATER:
            default:
                break;
        }
        return false;
    }
}

void ui_712_switch_to_sign(void) {
    nbgl_useCaseRegularReview(0, 0, "Reject", NULL, displaySignPage, reviewChoice);
}

void ui_712_start(void) {
    stringsIdx = 0;
    nbgl_useCaseRegularReview(0, 0, "Reject", NULL, displayTransactionPage, reviewChoice);
}

void ui_712_switch_to_message(void) {
    stringsIdx = 0;
    nbgl_useCaseRegularReview(0, 0, "Reject", NULL, displayTransactionPage, reviewChoice);
}
