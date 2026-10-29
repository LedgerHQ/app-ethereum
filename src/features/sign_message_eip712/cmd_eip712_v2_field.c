#include "status_words.h"
#include "cmd_eip712_v2_field.h"
#include "cmd_field.h"
#include "tlv_apdu.h"
#include "apdu_constants.h"
#include "eip712_session.h"
#include "operation_ctx.h"

uint16_t handle_eip712_v2_field(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    bool first_chunk = (p1 == P1_FIRST_CHUNK);

    if ((p2 != P2_EIP712_V2_IMPLEM) || (!first_chunk && (p1 != P1_FOLLOWING_CHUNK))) {
        return SWO_INCORRECT_P1_P2;
    }
    // the descriptors come with a message info, and only the ones it announces are accepted; and
    // they are not allowed once the review has started
    if (!eip712_session_is_preparing(EIP712_SESSION_V2) ||
        !operation_ctx_message_has_pending_fields()) {
        return SWO_COMMAND_NOT_ALLOWED;
    }
    if (!tlv_from_apdu(first_chunk, lc, payload, &handle_field_tlv_payload)) {
        return SWO_INCORRECT_DATA;
    }
    return SWO_SUCCESS;
}
