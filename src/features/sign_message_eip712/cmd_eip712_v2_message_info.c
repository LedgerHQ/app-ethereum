#include "status_words.h"
#include "cmd_eip712_v2_message_info.h"
#include "tlv_apdu.h"
#include "apdu_constants.h"
#include "eip712_v2_message_info.h"
#include "eip712_session.h"
#include "operation_ctx.h"
#include "typed_data.h"  // td_has_schema, td_has_domain, td_has_message

uint16_t handle_eip712_v2_message_info(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    bool first_chunk = (p1 == P1_FIRST_CHUNK);

    if ((p2 != P2_EIP712_V2_IMPLEM) || (!first_chunk && (p1 != P1_FOLLOWING_CHUNK))) {
        return SWO_INCORRECT_P1_P2;
    }
    // the message info is bound to the message, so the values have to be there; and it is not
    // allowed once the review has started
    if (!eip712_session_is_preparing(EIP712_SESSION_V2) || !td_has_schema() || !td_has_domain() ||
        !td_has_message()) {
        return SWO_COMMAND_NOT_ALLOWED;
    }
    // a message has one message info
    if (first_chunk && operation_ctx_has_message()) {
        return SWO_COMMAND_NOT_ALLOWED;
    }
    if (!tlv_from_apdu(first_chunk, lc, payload, &handle_eip712_v2_message_info_struct)) {
        return SWO_INCORRECT_DATA;
    }
    return SWO_SUCCESS;
}
