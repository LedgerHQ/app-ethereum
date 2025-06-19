#include "context_712.h"
#include "mem.h"
#include "mem_utils.h"
#include "sol_typenames.h"
#include "path.h"
#include "field_hash.h"
#include "ui_logic.h"
#include "typed_data.h"
#include "apdu_constants.h"  // APDU response codes
#include "shared_context.h"  // reset_app_context
#include "common_ui.h"       // ui_idle

e_struct_init struct_state = NOT_INITIALIZED;
s_eip712_context *eip712_context = NULL;

/**
 * Initialize the EIP712 context
 *
 * @return a boolean indicating if the initialization was successful or not
 */
bool eip712_context_init(void) {
    if (eip712_context != NULL) {
        eip712_context_deinit();
        return false;
    }

    // init global variables
    if ((eip712_context = app_mem_alloc(sizeof(*eip712_context))) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    explicit_bzero(eip712_context, sizeof(*eip712_context));

    if (sol_typenames_init() == false) {
        return false;
    }

    if (path_init() == false) {
        return false;
    }

    if (field_hash_init() == false) {
        return false;
    }

    if (ui_712_init() == false) {
        return false;
    }

    if (typed_data_init() == false) {
        return false;
    }

    eip712_context->go_home_on_failure = true;

    struct_state = NOT_INITIALIZED;

    return true;
}

/**
 * De-initialize the EIP712 context
 */
void eip712_context_deinit(void) {
    typed_data_deinit();
    path_deinit();
    field_hash_deinit();
    ui_712_deinit();
    sol_typenames_deinit();
    app_mem_free(eip712_context);
    eip712_context = NULL;
    reset_app_context();
}
