#ifdef HAVE_EIP712_FULL_SUPPORT

#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_logic.h"
#include "common_712.h"
#include "nbgl_use_case.h"
#include "network.h"
#include "ui_message_signing.h"
#include "ui_signing.h"

static nbgl_layoutTagValue_t pair;

static bool display_sign_page(uint8_t page, nbgl_pageContent_t *content) {
    (void) page;
    content->type = INFO_LONG_PRESS, content->infoLongPress.icon = get_app_icon(true);
    content->infoLongPress.text = TEXT_SIGN_EIP712;
    content->infoLongPress.longPressText = SIGN_BUTTON;
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
                                        NB_MAX_LINES_IN_REVIEW,
#if (API_LEVEL == 0 || API_LEVEL >= 14)
                                        &len,
                                        false);
#else
                                        &len);
#endif
            strings.tmp.tmp[len] = '\0';

            pair.item = strings.tmp.tmp2;
            pair.value = strings.tmp.tmp;
            content->type = TAG_VALUE_LIST;
            content->tagValueList.nbPairs = 1;
            content->tagValueList.pairs = &pair;
            content->tagValueList.wrapping = false;
            ret = true;
            break;

        case 1:
            if (ui_712_next_field() == EIP712_NO_MORE_FIELD) {
                ui_712_switch_to_sign();
            }
            __attribute__((fallthrough));
        default:
            ret = false;
            break;
    }
    return ret;
}

static void handle_display(nbgl_navCallback_t cb) {
    nbgl_useCaseRegularReview(0, 0, REJECT_BUTTON, NULL, cb, ui_message_review_choice);
}

void ui_712_start(void) {
    g_position = UI_SIGNING_POSITION_START;
    ui_message_start(TEXT_REVIEW_EIP712,
                     &ui_712_switch_to_message,
                     &ui_message_712_approved,
                     &ui_message_712_rejected);
}

void ui_712_switch_to_message(void) {
    g_position = UI_SIGNING_POSITION_REVIEW;
    handle_display(display_review_page);
}

void ui_712_switch_to_sign(void) {
    g_position = UI_SIGNING_POSITION_SIGN;
    handle_display(display_sign_page);
}

#endif  // HAVE_EIP712_FULL_SUPPORT
