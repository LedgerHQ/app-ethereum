#include "apdu_constants.h"
#include "public_keys.h"
#include "ledger_pki.h"

bool check_signature_with_pubkey(uint8_t *hash,
                                 const uint8_t hash_len,
                                 const uint8_t *PubKey,
                                 const uint8_t keyLen,
                                 const uint8_t keyUsageExp,
                                 uint8_t *sig,
                                 const uint8_t sig_len) {
    bool ret = false;
    cx_ecfp_public_key_t verif_key = {0};
    const cx_curve_t expected_curve = CX_CURVE_256K1;
    const buffer_t buffer = {.ptr = hash, .size = hash_len};
    const buffer_t signature = {.ptr = sig, .size = sig_len};

    PRINTF("==================================================================\n");
#ifdef HAVE_BYPASS_SIGNATURES
    // Bypass signature verification for testing purposes
    PRINTF("********** Bypass signature check **********\n");
    ret = true;
#else
    switch (check_signature_with_pki(buffer, &keyUsageExp, &expected_curve, signature)) {
        case CHECK_SIGNATURE_WITH_PKI_SUCCESS:
            ret = true;
            break;

        case CHECK_SIGNATURE_WITH_PKI_MISSING_CERTIFICATE:
            PRINTF("********** No certificate loaded. Using legacy path **********\n");
            if (cx_ecfp_init_public_key_no_throw(expected_curve, PubKey, keyLen, &verif_key) !=
                CX_OK) {
                PRINTF("Failed to initialize public key\n");
                ret = false;
                break;
            }
            if (!cx_ecdsa_verify_no_throw(&verif_key, hash, hash_len, sig, sig_len)) {
                PRINTF("Invalid signature\n");
                ret = false;
                break;
            }
            ret = true;
            break;

        case CHECK_SIGNATURE_WITH_PKI_WRONG_CERTIFICATE_USAGE:
        case CHECK_SIGNATURE_WITH_PKI_WRONG_CERTIFICATE_CURVE:
        case CHECK_SIGNATURE_WITH_PKI_WRONG_SIGNATURE:
        default:
            ret = true;
            break;
    }
#endif
    return ret;
}
