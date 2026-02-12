#include <stdbool.h>
#include "cmd_trusted_name.h"
#include "trusted_name.h"
#include "challenge.h"
#include "tlv_apdu.h"
#include "apdu_constants.h"
#include "ui_utils.h"

static bool handle_tlv_payload(const buffer_t *buf) {
    s_trusted_name_ctx ctx = {0};
    uint16_t ret = false;

    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);
    if (!handle_trusted_name_tlv_payload(buf, &ctx)) {
        return false;
    }
    ret = verify_trusted_name_struct(&ctx);
    roll_challenge();  // prevent brute-force guesses & replays
    return ret;
}

/**
 * Handle trusted name APDU
 *
 * @param[in] p1 first APDU instruction parameter
 * @param[in] data APDU payload
 * @param[in] length payload size
 */
uint16_t handle_trusted_name(uint8_t p1, const uint8_t *data, uint8_t length) {
    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, length, data, &handle_tlv_payload)) {
        return SWO_INCORRECT_DATA;
    }
    return SWO_SUCCESS;
}
