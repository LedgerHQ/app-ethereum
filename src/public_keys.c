#include <stdint.h>
#include <stdbool.h>
#include "ox_ec.h"
#include "buffer.h"
#include "ledger_pki.h"

bool check_signature_with_pubkey(uint8_t *hash,
                                 const uint8_t hash_len,
                                 const uint8_t key_usage,
                                 uint8_t *sig,
                                 const uint8_t sig_len) {
    bool ret = false;
    const cx_curve_t curve = CX_CURVE_256K1;
    const buffer_t buffer = {.ptr = hash, .size = hash_len};
    const buffer_t signature = {.ptr = sig, .size = sig_len};

    PRINTF("==================================================================\n");
#ifdef HAVE_BYPASS_SIGNATURES
    // Bypass signature verification for testing purposes
    PRINTF("********** Bypass signature check **********\n");
    ret = true;
#else
    if (check_signature_with_pki(buffer, &key_usage, &curve, signature) ==
        CHECK_SIGNATURE_WITH_PKI_SUCCESS) {
        ret = true;
    }
#endif
    return ret;
}
