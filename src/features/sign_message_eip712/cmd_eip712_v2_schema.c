#include "status_words.h"
#include "cmd_eip712_v2_schema.h"
#include "tlv_apdu.h"
#include "apdu_constants.h"
#include "eip712_v2_schema.h"
#include "typed_data.h"  // td_init
#include "eip712_session.h"

uint16_t handle_eip712_v2_schema(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    bool first_chunk = (p1 == P1_FIRST_CHUNK);

    if ((p2 != P2_EIP712_V2_IMPLEM) || (!first_chunk && (p1 != P1_FOLLOWING_CHUNK))) {
        return SWO_INCORRECT_P1_P2;
    }
    if (first_chunk) {
        // the schema opens the signing flow and creates the tree the rest of it fills
        if (!eip712_session_open(EIP712_SESSION_V2)) {
            return SWO_COMMAND_NOT_ALLOWED;
        }
        if (!td_init()) {
            return SWO_INSUFFICIENT_MEMORY;
        }
    } else if (!eip712_session_is_preparing(EIP712_SESSION_V2) || td_has_schema()) {
        // a complete schema is locked, no chunk may follow it
        return SWO_COMMAND_NOT_ALLOWED;
    }
    if (!tlv_from_apdu(first_chunk, lc, payload, &handle_eip712_v2_schema_struct)) {
        return SWO_INCORRECT_DATA;
    }
    return SWO_SUCCESS;
}
