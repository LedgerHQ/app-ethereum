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

static s_trusted_name trusted_name = {};

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
    hash_nbytes((uint8_t *) trusted_name.name, strlen(trusted_name.name), (cx_hash_t *) &hash_ctx);
    // chain id
    chain_id_be = __builtin_bswap64(*chain_id);
    hash_nbytes((uint8_t *) &chain_id_be, sizeof(chain_id_be), (cx_hash_t *) &hash_ctx);
    // key id
    hash_byte(trusted_name.key_id, (cx_hash_t *) &hash_ctx);
    // algo id
    hash_byte(trusted_name.algo_id, (cx_hash_t *) &hash_ctx);
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
                         trusted_name.sig,
                         trusted_name.sig_length)) {
#ifndef HAVE_BYPASS_SIGNATURES
        ret = false;
#endif
    } else {  // successful so we re-roll to prevent replays
        roll_challenge();
    }
    return ret;
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
    uint8_t off = 0;
    uint8_t name_length;

    (void) p1;
    (void) p2;

    if ((length - off) < sizeof(name_length)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return response_to_trusted_name(false, 0);
    }
    name_length = data[off];
    off += sizeof(name_length);

    if ((length - off) < name_length) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return response_to_trusted_name(false, 0);
    }
    memcpy((void *) trusted_name.name, &data[off], name_length);
    trusted_name.name[name_length] = '\0';
    off += name_length;

    if ((length - off) < sizeof(trusted_name.key_id)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return response_to_trusted_name(false, 0);
    }
    trusted_name.key_id = data[off];
    off += sizeof(trusted_name.key_id);

    if ((length - off) < sizeof(trusted_name.algo_id)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return response_to_trusted_name(false, 0);
    }
    trusted_name.algo_id = data[off];
    off += sizeof(trusted_name.algo_id);

    if ((length - off) < sizeof(trusted_name.sig_length)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return response_to_trusted_name(false, 0);
    }
    trusted_name.sig_length = data[off];
    off += sizeof(trusted_name.sig_length);

    if ((length - off) < trusted_name.sig_length) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return response_to_trusted_name(false, 0);
    }
    memcpy((void *) trusted_name.sig, &data[off], trusted_name.sig_length);
    off += trusted_name.sig_length;

    if ((length - off) > 0) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return response_to_trusted_name(false, 0);
    }
    return response_to_trusted_name(true, 0);
}

#endif  // HAVE_TRUSTED_NAME
