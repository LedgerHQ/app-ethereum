#include "common_ui.h"
#include "ui_signing.h"
#include "ui_nbgl.h"
#include "network.h"

typedef enum { PARAMETER_CONFIRMATION, SELECTOR_CONFIRMATION } e_confirmation_type;

enum {
    TOKEN_APPROVE = FIRST_USER_TOKEN,
};

static void reviewReject(void) {
    io_seproxyhal_touch_data_cancel(NULL);
}

static void long_press_cb(int token, uint8_t index, int page) {
    UNUSED(index);
    UNUSED(page);
    if (token == TOKEN_APPROVE) {
        io_seproxyhal_touch_data_ok(NULL);
    }
}

static void buildScreen(e_confirmation_type confirm_type) {
    static nbgl_genericContents_t contents = {0};
    static nbgl_content_t contentsList[3] = {0};
    static nbgl_contentTagValue_t pair = {0};
    uint8_t nbContents = 0;
    uint32_t buf_size = SHARED_BUFFER_SIZE / 2;

    snprintf(g_stax_shared_buffer,
             buf_size,
             "Verify %s",
             (confirm_type == PARAMETER_CONFIRMATION) ? "parameter" : "selector");
    // Finish text: replace "Verify" by "Confirm" and add questionmark
    snprintf(g_stax_shared_buffer + buf_size,
             buf_size,
             "Confirm %s",
             (confirm_type == PARAMETER_CONFIRMATION) ? "parameter" : "selector");

    pair.item = (confirm_type == PARAMETER_CONFIRMATION) ? "Parameter" : "Selector";
    pair.value = strings.tmp.tmp;

    // Title page
    contentsList[nbContents].type = CENTERED_INFO;
    contentsList[nbContents].content.centeredInfo.text1 = g_stax_shared_buffer;
    contentsList[nbContents].content.centeredInfo.icon = get_app_icon(true);
    contentsList[nbContents].content.centeredInfo.style = LARGE_CASE_INFO;
    nbContents++;

    // Values to be reviewed
    contentsList[nbContents].type = TAG_VALUE_LIST;
    contentsList[nbContents].content.tagValueList.pairs = &pair;
    contentsList[nbContents].content.tagValueList.nbPairs = 1;
    nbContents++;

    // Approval screen
    contentsList[nbContents].type = INFO_LONG_PRESS;
    contentsList[nbContents].content.infoLongPress.text = g_stax_shared_buffer + buf_size;
    contentsList[nbContents].content.infoLongPress.icon = get_app_icon(true);
    contentsList[nbContents].content.infoLongPress.longPressText = "Hold to confirm";
    contentsList[nbContents].content.infoLongPress.longPressToken = TOKEN_APPROVE;
    contentsList[nbContents].content.infoLongPress.tuneId = NB_TUNES;
    contentsList[nbContents].contentActionCallback = long_press_cb;
    nbContents++;

    contents.callbackCallNeeded = false;
    contents.contentsList = contentsList;
    contents.nbContents = nbContents;

    nbgl_useCaseGenericReview(&contents, REJECT_BUTTON, reviewReject);
}

void ui_confirm_parameter(void) {
    buildScreen(PARAMETER_CONFIRMATION);
}

void ui_confirm_selector(void) {
    buildScreen(SELECTOR_CONFIRMATION);
}
