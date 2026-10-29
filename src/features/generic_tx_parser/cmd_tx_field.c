#include "cmd_tx_field.h"
#include "cx.h"
#include "apdu_constants.h"
#include "tlv_apdu.h"
#include "cmd_field.h"
#include "operation_ctx.h"
#include "call_description.h"

static bool handle_tlv_payload(const buffer_t *buf) {
    if (!handle_field_tlv_payload(buf)) {
        return false;
    }
    while (call_description_is_nested() && validate_instruction_hash()) {
        if (!process_empty_calls_after() || !operation_ctx_pop()) {
            return false;
        }
    }
    return true;
}

uint16_t handle_tx_field(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    if (p2 != 0) {
        return SWO_INCORRECT_P1_P2;
    }
    if (!call_description_is_allowed(CALL_DESCRIPTION_FIELD)) {
        PRINTF("Error: no call can be described now!\n");
        return SWO_COMMAND_NOT_ALLOWED;
    }

    if (get_current_operation_info() == NULL) {
        // Do NOT call gcs_cleanup() here: if a GCS review is currently shown,
        // gcs_cleanup() frees g_pairs/g_pairsList while NBGL still holds
        // pointers to them, causing a crash.
        // Cleanup is handled in the approve/reject callbacks and in reset_app_context().
        PRINTF("Error: Field received without a TX info!\n");
        return SWO_COMMAND_NOT_ALLOWED;
    }

    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, lc, payload, &handle_tlv_payload)) {
        return SWO_INCORRECT_DATA;
    }
    return SWO_SUCCESS;
}
