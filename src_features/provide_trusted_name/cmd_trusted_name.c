#include <stdbool.h>
#include "cmd_trusted_name.h"
#include "trusted_name.h"
#include "challenge.h"
#include "tlv_apdu.h"
#include "apdu_constants.h"
#include "ui_utils.h"
#include "mem_utils.h"

#define TRUSTED_NAME_LEN (TRUSTED_NAME_MAX_LENGTH + 1)

static bool handle_tlv_payload(const uint8_t *payload, uint16_t size) {
    s_trusted_name_ctx ctx = {0};
    bool parsing_ret = false;
    bool verify_ret = false;

    // Allocate the Trusted Name buffer
    if (mem_buffer_allocate((void **) &g_trusted_name, TRUSTED_NAME_LEN) == false) {
        PRINTF("Memory allocation failed for Trusted Name buffer\n");
        return false;
    }

    ctx.trusted_name.name = g_trusted_name;
    cx_sha256_init(&ctx.hash_ctx);
    parsing_ret = tlv_parse(payload, size, (f_tlv_data_handler) &handle_trusted_name_struct, &ctx);
    verify_ret = verify_trusted_name_struct(&ctx);
    roll_challenge();  // prevent brute-force guesses & replays
    return (parsing_ret && verify_ret);
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
        mem_buffer_cleanup((void **) &g_trusted_name);
        mem_buffer_cleanup((void **) &g_trusted_name_info);
        return APDU_RESPONSE_INVALID_DATA;
    }
    return APDU_RESPONSE_OK;
}
