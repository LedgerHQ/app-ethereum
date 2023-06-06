#ifdef HAVE_EIP712_FULL_SUPPORT

#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_logic.h"
#include "common_712.h"
#include "nbgl_use_case.h"
#include "network.h"
#include "ui_message_signing.h"

static nbgl_layoutTagValue_t pair;

static bool display_sign_page(uint8_t page, nbgl_pageContent_t *content) {
    (void) page;
    content->type = INFO_LONG_PRESS, content->infoLongPress.icon = get_app_icon(true);
    content->infoLongPress.text = "Sign typed message";
    content->infoLongPress.longPressText = "Hold to sign";
    return true;
}

static bool display_review_page(uint8_t page, nbgl_pageContent_t *content) {
    bool ret;
    uint16_t len;

    switch (page) {
        case 0:
            // limit the value to one page
            nbgl_getTextMaxLenInNbLines(BAGL_FONT_INTER_MEDIUM_32px,
                                        strings.tmp.tmp,
                                        SCREEN_WIDTH - (2 * BORDER_MARGIN),
                                        9,
                                        &len);
            strings.tmp.tmp[len] = '\0';

            pair.item = strings.tmp.tmp2;
            pair.value = strings.tmp.tmp;
            content->type = TAG_VALUE_LIST;
            content->tagValueList.nbPairs = 1;
            content->tagValueList.pairs = &pair;
            content->tagValueList.wrapping = false;
            content->tagValueList.nbMaxLinesForValue = 0;
            ret = true;
            break;

        case 1:
            switch (ui_712_next_field()) {
                case EIP712_NO_MORE_FIELD:
                    return display_sign_page(page, content);
                    break;
                case EIP712_FIELD_INCOMING:
                case EIP712_FIELD_LATER:
                default:
                    break;
            }
            __attribute__((fallthrough));

        default:
            ret = false;
            break;
    }
    return ret;
}

static void handle_display(nbgl_navCallback_t cb) {
    nbgl_useCaseRegularReview(0, 0, "Reject", NULL, cb, ui_message_review_choice);
}

void ui_712_start(void) {
    ui_message_start("Review typed message",
                     NULL,
                     &ui_712_switch_to_message,
                     &ui_message_712_approved,
                     &ui_message_712_rejected);
}

void ui_712_switch_to_message(void) {
    handle_display(display_review_page);
}

void ui_712_switch_to_sign(void) {
    handle_display(display_sign_page);
}

#endif  // HAVE_EIP712_FULL_SUPPORT
