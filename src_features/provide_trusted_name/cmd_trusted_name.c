#ifdef HAVE_TRUSTED_NAME

#include <stdbool.h>
#include "cmd_trusted_name.h"
#include "trusted_name.h"
#include "mem.h"
#include "challenge.h"
#include "tlv_apdu.h"
#include "apdu_constants.h"

/**
 * Handle trusted name APDU
 *
 * @param[in] p1 first APDU instruction parameter
 * @param[in] data APDU payload
 * @param[in] length payload size
 */
uint16_t handle_trusted_name(uint8_t p1, const uint8_t *data, uint8_t length) {
    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, length, data, &handle_tlv_trusted_name_payload)) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    return APDU_RESPONSE_OK;
}

#endif  // HAVE_TRUSTED_NAME
