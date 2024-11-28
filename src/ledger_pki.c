#include "apdu_constants.h"
#include "public_keys.h"

#define KEY_USAGE_STR(x)                                                               \
    (x == CERTIFICATE_PUBLIC_KEY_USAGE_GENUINE_CHECK          ? "GENUINE_CHECK"        \
     : x == CERTIFICATE_PUBLIC_KEY_USAGE_EXCHANGE_PAYLOAD     ? "EXCHANGE_PAYLOAD"     \
     : x == CERTIFICATE_PUBLIC_KEY_USAGE_NFT_METADATA         ? "NFT_METADATA"         \
     : x == CERTIFICATE_PUBLIC_KEY_USAGE_TRUSTED_NAME         ? "TRUSTED_NAME"         \
     : x == CERTIFICATE_PUBLIC_KEY_USAGE_BACKUP_PROVIDER      ? "BACKUP_PROVIDER"      \
     : x == CERTIFICATE_PUBLIC_KEY_USAGE_RECOVER_ORCHESTRATOR ? "RECOVER_ORCHESTRATOR" \
     : x == CERTIFICATE_PUBLIC_KEY_USAGE_PLUGIN_METADATA      ? "PLUGIN_METADATA"      \
     : x == CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META            ? "COIN_META"            \
     : x == CERTIFICATE_PUBLIC_KEY_USAGE_SEED_ID_AUTH         ? "SEED_ID_AUTH"         \
                                                              : "Unknown")

int check_signature_with_pubkey(const char *tag,
                                uint8_t *buffer,
                                const uint8_t bufLen,
                                const uint8_t *PubKey,
                                const uint8_t keyLen,
#ifdef HAVE_LEDGER_PKI
                                const uint8_t keyUsageExp,
#endif
                                uint8_t *signature,
                                const uint8_t sigLen) {
    UNUSED(tag);
    cx_ecfp_public_key_t verif_key = {0};
    cx_err_t error = CX_INTERNAL_ERROR;
#ifdef HAVE_LEDGER_PKI
    uint8_t key_usage = 0;
    size_t trusted_name_len = 0;
    uint8_t trusted_name[CERTIFICATE_TRUSTED_NAME_MAXLEN] = {0};
    cx_ecfp_384_public_key_t public_key = {0};
#endif

    PRINTF(
        "[%s] "
        "=======================================================================================\n",
        tag);
#ifdef HAVE_LEDGER_PKI
    error = os_pki_get_info(&key_usage, trusted_name, &trusted_name_len, &public_key);
    if ((error == 0) && (key_usage == keyUsageExp)) {
        PRINTF("[%s] Certificate '%s' loaded for usage 0x%x (%s)\n",
               tag,
               trusted_name,
               key_usage,
               KEY_USAGE_STR(key_usage));

        // Checking the signature with PKI
        if (!os_pki_verify(buffer, bufLen, signature, sigLen)) {
            PRINTF("%s: Invalid signature\n", tag);
#ifndef HAVE_BYPASS_SIGNATURES
            error = APDU_RESPONSE_INVALID_DATA;
            goto end;
#endif
        }
    } else
#endif
    {
        PRINTF("[%s] ********** No certificate loaded. Using legacy path **********\n", tag);
        CX_CHECK(cx_ecfp_init_public_key_no_throw(CX_CURVE_256K1, PubKey, keyLen, &verif_key));
        if (!cx_ecdsa_verify_no_throw(&verif_key, buffer, bufLen, signature, sigLen)) {
            PRINTF("%s: Invalid signature\n", tag);
#ifndef HAVE_BYPASS_SIGNATURES
            error = APDU_RESPONSE_INVALID_DATA;
            goto end;
#endif
        }
    }
    error = CX_OK;
end:
    return error;
}
