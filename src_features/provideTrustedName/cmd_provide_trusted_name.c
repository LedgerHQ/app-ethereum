#ifdef HAVE_TRUSTED_NAME

#include <os.h>
#include <stdint.h>
#include <string.h>
#include "apdu_constants.h"
#include "trusted_name.h"
#include "challenge.h"
#include "mem.h"
#include "mem_utils.h"
#include "hash_bytes.h"

typedef enum { STRUCT_TYPE_1 = 0x00 } e_ens_struct_type;
typedef enum { APDU_FORMAT_VERSION_1 = 0x00 } e_ens_apdu_format_version;
typedef enum {
    APDU_STATE_NAME_LENGTH = 0,
    APDU_STATE_NAME,
    APDU_STATE_KEY_ID,
    APDU_STATE_ALGO_ID,
    APDU_STATE_SIG_LENGTH,
    APDU_STATE_SIG,
    APDU_STATE_COUNT
} e_apdu_state;
typedef bool (*t_apdu_parsing_func)(const uint8_t *, uint8_t, uint8_t *);

char trusted_name[TRUSTED_NAME_MAX_LENGTH + 1] = {0};
static s_trusted_name trusted_name_info = {0};
static uint8_t apdu_state = APDU_STATE_NAME_LENGTH;
static uint8_t current_val_offset = 0;

static const uint8_t TRUSTED_NAME_PUB_KEY[] = {
#ifdef HAVE_TRUSTED_NAME_TEST_KEY
    0x04, 0xb9, 0x1f, 0xbe, 0xc1, 0x73, 0xe3, 0xba, 0x4a, 0x71, 0x4e, 0x01, 0x4e, 0xbc,
    0x82, 0x7b, 0x6f, 0x89, 0x9a, 0x9f, 0xa7, 0xf4, 0xac, 0x76, 0x9c, 0xde, 0x28, 0x43,
    0x17, 0xa0, 0x0f, 0x4f, 0x65, 0x0f, 0x09, 0xf0, 0x9a, 0xa4, 0xff, 0x5a, 0x31, 0x76,
    0x02, 0x55, 0xfe, 0x5d, 0xfc, 0x81, 0x13, 0x29, 0xb3, 0xb5, 0x0b, 0xe9, 0x91, 0x94,
    0xfc, 0xa1, 0x16, 0x19, 0xe6, 0x5f, 0x2e, 0xdf, 0xea
#else
    0x04, 0x6a, 0x94, 0xe7, 0xa4, 0x2c, 0xd0, 0xc3, 0x3f, 0xdf, 0x44, 0x0c, 0x8e, 0x2a,
    0xb2, 0x54, 0x2c, 0xef, 0xbe, 0x5d, 0xb7, 0xaa, 0x0b, 0x93, 0xa9, 0xfc, 0x81, 0x4b,
    0x9a, 0xcf, 0xa7, 0x5e, 0xb4, 0xe5, 0x3d, 0x6f, 0x00, 0x25, 0x94, 0xbd, 0xb6, 0x05,
    0xd9, 0xb5, 0xbd, 0xa9, 0xfa, 0x4b, 0x4b, 0xf3, 0xa5, 0x49, 0x6f, 0xd3, 0x16, 0x4b,
    0xae, 0xf5, 0xaf, 0xcf, 0x90, 0xe8, 0x40, 0x88, 0x71
#endif
};

/**
 * Send a response APDU
 *
 * @param[in] success whether it should use \ref APDU_RESPONSE_OK
 * @param[in] off payload offset (0 if no data other than status word)
 */
static void response_to_trusted_name(bool success, uint8_t off) {
    uint16_t sw;

    if (success) {
        sw = APDU_RESPONSE_OK;
    } else {
        sw = apdu_response_code;
    }
    U2BE_ENCODE(G_io_apdu_buffer, off, sw);

    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, off + 2);
}

/**
 * Verify the trusted name in memory with the given address and chain ID
 *
 * @param[in] chain_id given chain ID
 * @param[in] addr given address
 * @return whether it matches or not
 */
bool verify_trusted_name(const uint64_t *chain_id, const uint8_t *addr) {
    bool ret = true;
    uint8_t hash[INT256_LENGTH];
    cx_ecfp_public_key_t verif_key;
    cx_sha256_t hash_ctx;
    uint32_t chal;
    uint64_t chain_id_be;

    cx_sha256_init(&hash_ctx);
    // challenge
    chal = __builtin_bswap32(get_challenge());
    hash_nbytes((uint8_t *) &chal, sizeof(chal), (cx_hash_t *) &hash_ctx);
    // address
    hash_nbytes(addr, ADDRESS_LENGTH, (cx_hash_t *) &hash_ctx);
    // name
    hash_nbytes((uint8_t *) trusted_name, strlen(trusted_name), (cx_hash_t *) &hash_ctx);
    // chain id
    chain_id_be = __builtin_bswap64(*chain_id);
    hash_nbytes((uint8_t *) &chain_id_be, sizeof(chain_id_be), (cx_hash_t *) &hash_ctx);
    // key id
    hash_byte(trusted_name_info.key_id, (cx_hash_t *) &hash_ctx);
    // algo id
    hash_byte(trusted_name_info.algo_id, (cx_hash_t *) &hash_ctx);
    cx_hash((cx_hash_t *) &hash_ctx, CX_LAST, NULL, 0, hash, INT256_LENGTH);

    cx_ecfp_init_public_key(CX_CURVE_256K1,
                            TRUSTED_NAME_PUB_KEY,
                            sizeof(TRUSTED_NAME_PUB_KEY),
                            &verif_key);
    if (!cx_ecdsa_verify(&verif_key,
                         CX_LAST,
                         CX_SHA256,
                         hash,
                         sizeof(hash),
                         trusted_name_info.sig,
                         trusted_name_info.sig_length)) {
#ifndef HAVE_BYPASS_SIGNATURES
        ret = false;
#endif
    } else {  // successful so we re-roll to prevent replays
        roll_challenge();
    }
    return ret;
}

/**
 * Parse the domain name length from the APDU payload
 *
 * @param[in] data payload
 * @param[in] length size of payload
 * @param[in,out] apdu_offset offset to the currently processed byte(s) in the payload
 * @return whether the parsing was successful
 */
static bool parse_name_length(const uint8_t *data, uint8_t length, uint8_t *apdu_offset) {
    if ((length - *apdu_offset) < sizeof(trusted_name_info.name_length)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        response_to_trusted_name(false, 0);
        return false;
    }
    trusted_name_info.name_length = data[*apdu_offset];
    if (trusted_name_info.name_length > TRUSTED_NAME_MAX_LENGTH) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA + 1;
        response_to_trusted_name(false, 0);
        return false;
    }
    *apdu_offset += sizeof(trusted_name_info.name_length);
    trusted_name[trusted_name_info.name_length] = '\0';
    current_val_offset = 0;
    apdu_state = APDU_STATE_NAME;
    return true;
}

/**
 * Parse the domain name from the APDU payload
 *
 * @param[in] data payload
 * @param[in] length size of payload
 * @param[in,out] apdu_offset offset to the currently processed byte(s) in the payload
 * @return whether the parsing was successful
 */
static bool parse_name(const uint8_t *data, uint8_t length, uint8_t *apdu_offset) {
    uint8_t cpy_length =
        MIN(trusted_name_info.name_length - current_val_offset, length - *apdu_offset);
    memcpy((void *) trusted_name + current_val_offset, &data[*apdu_offset], cpy_length);
    current_val_offset += cpy_length;
    *apdu_offset += cpy_length;
    if (current_val_offset == trusted_name_info.name_length) {
        apdu_state = APDU_STATE_KEY_ID;
    }
    return true;
}

/**
 * Parse the key ID from the APDU payload
 *
 * @param[in] data payload
 * @param[in] length size of payload
 * @param[in,out] apdu_offset offset to the currently processed byte(s) in the payload
 * @return whether the parsing was successful
 */
static bool parse_key_id(const uint8_t *data, uint8_t length, uint8_t *apdu_offset) {
    if ((length - *apdu_offset) < sizeof(trusted_name_info.key_id)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA + 2;
        response_to_trusted_name(false, 0);
        return false;
    }
    trusted_name_info.key_id = data[*apdu_offset];
    *apdu_offset += sizeof(trusted_name_info.key_id);
    apdu_state = APDU_STATE_ALGO_ID;
    return true;
}

/**
 * Parse the algorithm ID from the APDU payload
 *
 * @param[in] data payload
 * @param[in] length size of payload
 * @param[in,out] apdu_offset offset to the currently processed byte(s) in the payload
 * @return whether the parsing was successful
 */
static bool parse_algo_id(const uint8_t *data, uint8_t length, uint8_t *apdu_offset) {
    if ((length - *apdu_offset) < sizeof(trusted_name_info.algo_id)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA + 3;
        response_to_trusted_name(false, 0);
        return false;
    }
    trusted_name_info.algo_id = data[*apdu_offset];
    *apdu_offset += sizeof(trusted_name_info.algo_id);
    apdu_state = APDU_STATE_SIG_LENGTH;
    return true;
}

/**
 * Parse the signature length from the APDU payload
 *
 * @param[in] data payload
 * @param[in] length size of payload
 * @param[in,out] apdu_offset offset to the currently processed byte(s) in the payload
 * @return whether the parsing was successful
 */
static bool parse_sig_length(const uint8_t *data, uint8_t length, uint8_t *apdu_offset) {
    if ((length - *apdu_offset) < sizeof(trusted_name_info.sig_length)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA + 4;
        response_to_trusted_name(false, 0);
        return false;
    }
    trusted_name_info.sig_length = data[*apdu_offset];
    if (trusted_name_info.sig_length > ECDSA_SIG_MAX_LENGTH) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA + 5;
        response_to_trusted_name(false, 0);
        return false;
    }
    *apdu_offset += sizeof(trusted_name_info.sig_length);
    current_val_offset = 0;
    apdu_state = APDU_STATE_SIG;
    return true;
}

/**
 * Parse the signature from the APDU payload
 *
 * @param[in] data payload
 * @param[in] length size of payload
 * @param[in,out] apdu_offset offset to the currently processed byte(s) in the payload
 * @return whether the parsing was successful
 */
static bool parse_sig(const uint8_t *data, uint8_t length, uint8_t *apdu_offset) {
    uint8_t cpy_length =
        MIN(trusted_name_info.sig_length - current_val_offset, length - *apdu_offset);
    memcpy((void *) trusted_name_info.sig + current_val_offset, &data[*apdu_offset], cpy_length);
    current_val_offset += cpy_length;
    *apdu_offset += cpy_length;
    if (current_val_offset == trusted_name_info.sig_length) {
        apdu_state = APDU_STATE_NAME_LENGTH;
    }
    return true;
}

/**
 * Handle trusted name APDU
 *
 * @param[in] p1 first APDU instruction parameter
 * @param[in] p2 second APDU instruction parameter
 * @param[in] data APDU payload
 * @param[in] length payload size
 */
void handle_provide_trusted_name(uint8_t p1, uint8_t p2, const uint8_t *data, uint8_t length) {
    t_apdu_parsing_func parsing_funcs[APDU_STATE_COUNT] =
        {parse_name_length, parse_name, parse_key_id, parse_algo_id, parse_sig_length, parse_sig};
    uint8_t apdu_offset = 0;

    (void) p1;
    (void) p2;
    while (apdu_offset < length) {
        if (apdu_state >= APDU_STATE_COUNT) break;
        if (!(*(t_apdu_parsing_func) PIC(parsing_funcs[apdu_state]))(data, length, &apdu_offset)) {
            return;
        }
    }
    return response_to_trusted_name(apdu_offset == length, 0);
}

#endif  // HAVE_TRUSTED_NAME
