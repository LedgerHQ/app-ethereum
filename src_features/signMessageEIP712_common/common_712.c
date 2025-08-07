#include "apdu_constants.h"
#include "os_io_seproxyhal.h"
#include "crypto_helpers.h"
#include "ui_callbacks.h"
#include "common_712.h"

static const uint8_t EIP_712_MAGIC[] = {0x19, 0x01};

unsigned int ui_712_approve_cb(void) {
    uint8_t hash[INT256_LENGTH];

    io_seproxyhal_io_heartbeat();
    CX_ASSERT(cx_keccak_init_no_throw(&global_sha3, 256));
    CX_ASSERT(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                               0,
                               (uint8_t *) EIP_712_MAGIC,
                               sizeof(EIP_712_MAGIC),
                               NULL,
                               0));
    CX_ASSERT(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                               0,
                               tmpCtx.messageSigningContext712.domainHash,
                               sizeof(tmpCtx.messageSigningContext712.domainHash),
                               NULL,
                               0));
    CX_ASSERT(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                               CX_LAST,
                               tmpCtx.messageSigningContext712.messageHash,
                               sizeof(tmpCtx.messageSigningContext712.messageHash),
                               hash,
                               sizeof(hash)));
    PRINTF("EIP712 Domain hash 0x%.*h\n", 32, tmpCtx.messageSigningContext712.domainHash);
    PRINTF("EIP712 Message hash 0x%.*h\n", 32, tmpCtx.messageSigningContext712.messageHash);

    unsigned int info = 0;
    CX_ASSERT(bip32_derive_ecdsa_sign_rs_hash_256(CX_CURVE_256K1,
                                                  tmpCtx.messageSigningContext712.bip32.path,
                                                  tmpCtx.messageSigningContext712.bip32.length,
                                                  CX_RND_RFC6979 | CX_LAST,
                                                  CX_SHA256,
                                                  hash,
                                                  sizeof(hash),
                                                  G_io_apdu_buffer + 1,
                                                  G_io_apdu_buffer + 1 + 32,
                                                  &info));
    G_io_apdu_buffer[0] = 27;
    if (info & CX_ECCINFO_PARITY_ODD) {
        G_io_apdu_buffer[0]++;
    }
    if (info & CX_ECCINFO_xGTn) {
        G_io_apdu_buffer[0] += 2;
    }
    return io_seproxyhal_send_status(APDU_RESPONSE_OK, 65, true, false);
}

unsigned int ui_712_reject_cb(void) {
    return io_seproxyhal_send_status(APDU_RESPONSE_CONDITION_NOT_SATISFIED, 0, true, false);
}

static char *format_hash(const uint8_t *hash, char *buffer, size_t buffer_size, size_t offset) {
    array_bytes_string(buffer + offset, buffer_size - offset, hash, KECCAK256_HASH_BYTESIZE);
    return buffer + offset;
}

void eip712_format_hash(nbgl_contentTagValue_t *pairs,
                        uint8_t nbPairs,
                        nbgl_contentTagValueList_t *pairs_list) {
    explicit_bzero(pairs, sizeof(nbgl_contentTagValue_t) * nbPairs);
    explicit_bzero(pairs_list, sizeof(nbgl_contentTagValueList_t));

    if (nbPairs < 2) {
        return;  // Ensure we have enough space for two pairs
    }
    pairs[0].item = "Domain hash";
    pairs[0].value = format_hash(tmpCtx.messageSigningContext712.domainHash,
                                 strings.tmp.tmp,
                                 sizeof(strings.tmp.tmp),
                                 0);
    pairs[1].item = "Message hash";
    pairs[1].value = format_hash(tmpCtx.messageSigningContext712.messageHash,
                                 strings.tmp.tmp,
                                 sizeof(strings.tmp.tmp),
                                 70);

    pairs_list->nbPairs = 2;
    pairs_list->pairs = pairs;
    pairs_list->nbMaxLinesForValue = 0;
}
