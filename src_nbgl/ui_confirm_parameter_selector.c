#include "ui_nbgl.h"
#include "ui_callbacks.h"
#include "ui_utils.h"

#define TITLE_MSG_LEN  20
#define FINISH_MSG_LEN 20

typedef enum { PARAMETER_CONFIRMATION, SELECTOR_CONFIRMATION } e_confirmation_type;

static void reviewChoice(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_data_ok();
    } else {
        io_seproxyhal_touch_data_cancel();
    }
    // No cleanup here, will be done with transaction review
}

static void buildScreen(e_confirmation_type confirm_type) {
    nbgl_operationType_t op = TYPE_TRANSACTION;

    // Initialize the buffers
    if (!ui_pairs_init(1)) {
        // Initialization failed, cleanup and return
        return;
    }
    // Initialize the buffers
    if (!ui_buffers_init(TITLE_MSG_LEN, 0, FINISH_MSG_LEN)) {
        // Initialization failed, cleanup and return
        return;
    }

    g_pairs->item = (confirm_type == PARAMETER_CONFIRMATION) ? "Parameter" : "Selector";
    g_pairs->value = strings.tmp.tmp;

    snprintf(g_titleMsg,
             TITLE_MSG_LEN,
             "Verify %s",
             (confirm_type == PARAMETER_CONFIRMATION) ? "parameter" : "selector");
    // Finish text: replace "Verify" by "Confirm"
    snprintf(g_finishMsg,
             FINISH_MSG_LEN,
             "Confirm %s",
             (confirm_type == PARAMETER_CONFIRMATION) ? "parameter" : "selector");

    if (tmpContent.txContent.dataPresent) {
        op |= BLIND_OPERATION;
    }
    nbgl_useCaseReview(op,
                       g_pairsList,
                       get_tx_icon(false),
                       g_titleMsg,
                       NULL,
                       g_finishMsg,
                       reviewChoice);
}

void ui_confirm_parameter(void) {
    buildScreen(PARAMETER_CONFIRMATION);
}

void ui_confirm_selector(void) {
    buildScreen(SELECTOR_CONFIRMATION);
}
