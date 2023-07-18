#include "common_ui.h"
#include "ui_signing.h"
#include "ui_nbgl.h"
#include "network.h"

typedef enum { PARAMETER_CONFIRMATION, SELECTOR_CONFIRMATION } e_confirmation_type;

static nbgl_layoutTagValue_t pair;
static e_confirmation_type confirm_type;

static void reviewReject(void) {
    io_seproxyhal_touch_data_cancel(NULL);
}

static void confirmTransation(void) {
    io_seproxyhal_touch_data_ok(NULL);
}

static void reviewChoice(bool confirm) {
    if (confirm) {
        confirmTransation();
    } else {
        reviewReject();
    }
}

static bool displayTransactionPage(uint8_t page, nbgl_pageContent_t *content) {
    if (page == 0) {
        pair.item = (confirm_type == PARAMETER_CONFIRMATION) ? "Parameter" : "Selector";
        pair.value = strings.tmp.tmp;
        content->type = TAG_VALUE_LIST;
        content->tagValueList.nbPairs = 1;
        content->tagValueList.pairs = (nbgl_layoutTagValue_t *) &pair;
    } else if (page == 1) {
        snprintf(g_stax_shared_buffer,
                 sizeof(g_stax_shared_buffer),
                 "Confirm %s",
                 (confirm_type == PARAMETER_CONFIRMATION) ? "parameter" : "selector");
        content->type = INFO_LONG_PRESS, content->infoLongPress.icon = get_app_icon(true);
        content->infoLongPress.text = g_stax_shared_buffer;
        content->infoLongPress.longPressText = "Hold to confirm";
    } else {
        return false;
    }
    // valid page so return true
    return true;
}

static void reviewContinue(void) {
    snprintf(g_stax_shared_buffer,
             sizeof(g_stax_shared_buffer),
             "Reject %s",
             (confirm_type == PARAMETER_CONFIRMATION) ? "parameter" : "selector");
    nbgl_useCaseRegularReview(0,
                              2,
                              g_stax_shared_buffer,
                              NULL,
                              displayTransactionPage,
                              reviewChoice);
}

static void buildScreen(void) {
    snprintf(g_stax_shared_buffer,
             sizeof(g_stax_shared_buffer),
             "Verify %s",
             (confirm_type == PARAMETER_CONFIRMATION) ? "parameter" : "selector");
    nbgl_useCaseReviewStart(get_app_icon(true),
                            g_stax_shared_buffer,
                            NULL,
                            REJECT_BUTTON,
                            reviewContinue,
                            reviewReject);
}

void ui_confirm_parameter(void) {
    confirm_type = PARAMETER_CONFIRMATION;
    buildScreen();
}

void ui_confirm_selector(void) {
    confirm_type = SELECTOR_CONFIRMATION;
    buildScreen();
}
