#include "status_words.h"
#include "cmd_eip712_v2_values.h"
#include "tlv_apdu.h"
#include "apdu_constants.h"
#include "shared_context.h"
#include "eip712_v2_values.h"
#include "typed_data.h"  // td_has_schema, td_has_domain, td_has_message

uint16_t handle_eip712_v2_values(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    bool first_chunk = (p1 == P1_FIRST_CHUNK);

    if ((p2 != P2_EIP712_V2_IMPLEM) || (!first_chunk && (p1 != P1_FOLLOWING_CHUNK))) {
        return SWO_INCORRECT_P1_P2;
    }
    // the schema must have been received first, which is what left the app in this state
    if (appState != APP_STATE_PREPARING_EIP712) {
        return SWO_COMMAND_NOT_ALLOWED;
    }
    // the values are only meaningful against a complete schema, and are accepted once
    if (first_chunk && (!td_has_schema() || td_has_domain() || td_has_message())) {
        return SWO_COMMAND_NOT_ALLOWED;
    }
    if (!tlv_from_apdu(first_chunk, lc, payload, &handle_eip712_v2_values_struct)) {
        return SWO_INCORRECT_DATA;
    }
    return SWO_SUCCESS;
}
