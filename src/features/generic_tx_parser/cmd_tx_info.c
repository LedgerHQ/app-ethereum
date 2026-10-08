#include <string.h>
#include "cmd_tx_info.h"
#include "apdu_constants.h"
#include "operation_info.h"
#include "tlv_apdu.h"
#include "operation_ctx.h"
#include "call_description.h"
#include "gcs_field_table.h"  // set_intent_field

/**
 * Check a transaction description is about the calldata parked for the transaction it opens
 *
 * The descriptions that follow are matched to their own transaction context, selector included,
 * by find_matching_operation_ctx().
 *
 * @param[in] tx_info the description
 * @return whether it can go on
 */
static bool selector_matches_parked_calldata(const s_operation_info *tx_info) {
    const uint8_t *selector;

    if (get_operation_ctx_count() != 0) {
        return true;
    }
    if ((selector = calldata_get_selector(g_parked_calldata)) == NULL) {
        return false;
    }
    if (memcmp(selector, get_selector(tx_info), CALLDATA_SELECTOR_SIZE) != 0) {
        PRINTF("Error: selector mismatch!\n");
        return false;
    }
    return true;
}

/**
 * Start the review of a nested call, which is done with it if it announces no descriptors
 *
 * @param[in] info the description of the call
 * @return whether it was successful
 */
static bool start_nested_call(const s_operation_info *info) {
    bool no_fields;

    if (!call_description_is_nested()) {
        return true;
    }
    if (!set_intent_field(info->operation_type) ||
        !operation_info_fields_hash_is_empty(info, &no_fields)) {
        return false;
    }
    if (no_fields && !operation_ctx_pop()) {
        return false;
    }
    return true;
}

static bool handle_tlv_payload(const buffer_t *buf) {
    s_operation_info *tx_info;

    if ((tx_info = new_operation_info(buf, OPERATION_INFO_CTX_SIGN_TX)) == NULL) {
        return false;
    }
    if (!selector_matches_parked_calldata(tx_info)) {
        delete_operation_info(tx_info);
        return false;
    }
    if (!find_matching_operation_ctx(tx_info->contract_addr,
                                     get_selector(tx_info),
                                     &tx_info->chain_id)) {
        delete_operation_info(tx_info);
        return false;
    }
    if (!process_empty_calls_before()) {
        delete_operation_info(tx_info);
        return false;
    }
    // Attached to the matched context before any check, freed with it
    if (!operation_ctx_set_info(tx_info)) {
        delete_operation_info(tx_info);
        return false;
    }
    return start_nested_call(tx_info);
}

uint16_t handle_tx_info(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    if (p2 != 0) {
        return SWO_INCORRECT_P1_P2;
    }
    if ((appState != APP_STATE_SIGNING_TX) && (appState != APP_STATE_SIGNING_EIP712)) {
        PRINTF("App not in TX signing mode!\n");
        return SWO_COMMAND_NOT_ALLOWED;
    }
    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, lc, payload, &handle_tlv_payload)) {
        return SWO_INCORRECT_DATA;
    }
    return SWO_SUCCESS;
}
