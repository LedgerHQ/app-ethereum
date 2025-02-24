#ifdef HAVE_TRUSTED_NAME

#include <stdbool.h>
#include "cmd_trusted_name.h"
#include "trusted_name.h"
#include "mem.h"
#include "challenge.h"
#include "tlv_apdu.h"
#include "apdu_constants.h"

static bool handle_tlv_payload(const uint8_t *payload, uint16_t size, bool to_free) {
    s_trusted_name_ctx ctx = {0};
    bool parsing_success;

    g_trusted_name_info.name = g_trusted_name;
    ctx.info = &g_trusted_name_info;
    cx_sha256_init(&ctx.hash_ctx);
    parsing_success =
        tlv_parse(payload, size, (f_tlv_data_handler) &handle_trusted_name_struct, &ctx);
    if (to_free) mem_dealloc(size);
    if (!parsing_success || !verify_trusted_name_struct(&ctx) ||
        !verify_trusted_name_signature(&ctx)) {
        roll_challenge();  // prevent brute-force guesses
        explicit_bzero(&g_trusted_name_info, sizeof(g_trusted_name_info));
        return false;
    }
    roll_challenge();  // prevent replays
    return true;
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
        return APDU_RESPONSE_INVALID_DATA;
    }
    return APDU_RESPONSE_OK;
}

#endif  // HAVE_TRUSTED_NAME
