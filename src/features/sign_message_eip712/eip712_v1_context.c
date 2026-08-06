#include "eip712_v1_context.h"
#include "app_mem_utils.h"
#include "mem_utils.h"
#include "eip712_v1_ui_logic.h"
#include "eip712_v1_parse.h"
#include "typed_data.h"
#include "shared_context.h"  // reset_app_context
#include "common_ui.h"       // ui_idle

s_eip712_v1_context *eip712_v1_context = NULL;

/**
 * Initialize the EIP712 context
 *
 * @return a boolean indicating if the initialization was successful or not
 */
bool eip712_v1_context_init(void) {
    if (eip712_v1_context != NULL) {
        reset_app_context();
        return false;
    }

    // init global variables
    if (APP_MEM_CALLOC((void **) &eip712_v1_context, sizeof(*eip712_v1_context)) == false) {
        return false;
    }

    if (ui_712_init() == false) {
        return false;
    }

    if (td_init() == false) {
        return false;
    }

    appState = APP_STATE_PREPARING_EIP712;
    return true;
}

/**
 * De-initialize the EIP712 context
 *
 * Only tears down the EIP-712-specific state. \ref reset_app_context calls this itself
 * to fold it into the generic app reset; this must not call back into it.
 */
void eip712_v1_context_deinit(void) {
    v1_parse_deinit();
    td_deinit();
    ui_712_deinit();
    APP_MEM_FREE_AND_NULL((void **) &eip712_v1_context);
}
