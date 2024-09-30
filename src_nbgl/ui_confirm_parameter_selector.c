#include "common_ui.h"
#include "ui_signing.h"
#include "ui_nbgl.h"
#include "network.h"

typedef enum { PARAMETER_CONFIRMATION, SELECTOR_CONFIRMATION } e_confirmation_type;
static nbgl_contentTagValue_t pair;
static nbgl_contentTagValueList_t pairsList;

static void reviewChoice(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_data_ok();
    } else {
        io_seproxyhal_touch_data_cancel();
    }
}

static void buildScreen(e_confirmation_type confirm_type) {
    uint32_t buf_size = SHARED_BUFFER_SIZE / 2;
    nbgl_operationType_t op = TYPE_TRANSACTION;

    pair.item = (confirm_type == PARAMETER_CONFIRMATION) ? "Parameter" : "Selector";
    pair.value = strings.tmp.tmp;
    pairsList.nbPairs = 1;
    pairsList.pairs = &pair;
    snprintf(g_stax_shared_buffer,
             buf_size,
             "Verify %s",
             (confirm_type == PARAMETER_CONFIRMATION) ? "parameter" : "selector");
    // Finish text: replace "Verify" by "Confirm" and add questionmark
    snprintf(g_stax_shared_buffer + buf_size,
             buf_size,
             "Confirm %s",
             (confirm_type == PARAMETER_CONFIRMATION) ? "parameter" : "selector");

    if (tmpContent.txContent.dataPresent) {
        op |= BLIND_OPERATION;
    }
    nbgl_useCaseReview(op,
                       &pairsList,
                       get_tx_icon(),
                       g_stax_shared_buffer,
                       NULL,
                       g_stax_shared_buffer + buf_size,
                       reviewChoice);
}

void ui_confirm_parameter(void) {
    buildScreen(PARAMETER_CONFIRMATION);
}

void ui_confirm_selector(void) {
    buildScreen(SELECTOR_CONFIRMATION);
}
