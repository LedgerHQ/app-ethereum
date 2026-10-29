#include "apdu_constants.h"
#include "signature_check.h"
#include "ledger_pki.h"

/**
 * Verify a signature with the public key of the certificate loaded through Ledger PKI.
 *
 * @param[in] hash hash to verify
 * @param[in] hash_len hash length
 * @param[in] keyUsageExp expected key usage of the loaded certificate
 * @param[in] sig DER-encoded signature
 * @param[in] sig_len signature length
 * @return whether the signature is valid
 */
bool check_signature_with_pubkey(uint8_t *hash,
                                 const uint8_t hash_len,
                                 const uint8_t keyUsageExp,
                                 const uint8_t *sig,
                                 const uint8_t sig_len) {
#ifdef HAVE_BYPASS_SIGNATURES
    UNUSED(hash);
    UNUSED(hash_len);
    UNUSED(keyUsageExp);
    UNUSED(sig);
    UNUSED(sig_len);
    // Bypass signature verification for testing purposes
    PRINTF("********** Bypass signature check **********\n");
    return true;
#else
    const cx_curve_t expected_curve = CX_CURVE_256K1;
    const buffer_t buffer = {.ptr = hash, .size = hash_len};
    const buffer_t signature = {.ptr = (uint8_t *) sig, .size = sig_len};

    return check_signature_with_pki(buffer, &keyUsageExp, &expected_curve, signature) ==
           CHECK_SIGNATURE_WITH_PKI_SUCCESS;
#endif
}
