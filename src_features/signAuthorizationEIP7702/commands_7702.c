#include "shared_context.h"
#include "apdu_constants.h"
#include "tlv_apdu.h"
#include "common_ui.h"
#include "network.h"
#include "crypto_helpers.h"
#include "write.h"
#include "commands_7702.h"
#include "shared_7702.h"
#include "rlp_encode.h"
#include "whitelist_7702.h"
#include "auth_7702.h"
#include "getPublicKey.h"
#include "mem_utils.h"

// Avoid saving the full structure when parsing
// Alternative option : add a callback to f_tlv_payload_handler
static uint16_t g_7702_sw;
cx_sha3_t *g_7702_hash_ctx = NULL;

#define MAGIC_7702 5

/**
 * @brief Encode a parameter as RLP and hash it using Keccak
 * @param [in] data parameter to hash
 * @param [in] dataLength size of the parameter to hash
 * @param [out] rlpTmp temporary buffer to store the RLP encoded parameter
 * @param [in] rlpTmpLength size of the temporary buffer to store the RLP encoded parameter
 * @return APDU_NO_RESPONSE if the parameter could be hashed, or an error code
 */
uint16_t hashRLP(const uint8_t *data, uint8_t dataLength, uint8_t *rlpTmp, uint8_t rlpTmpLength) {
    cx_err_t error = CX_INTERNAL_ERROR;
    uint8_t hashSize;

    hashSize = rlpEncodeNumber(data, dataLength, rlpTmp, rlpTmpLength);
    if (hashSize == 0) {
        return APDU_RESPONSE_UNKNOWN;
    }
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) g_7702_hash_ctx, 0, rlpTmp, hashSize, NULL, 0));
    return APDU_NO_RESPONSE;
end:
    return error;
}

/**
 * @brief Encode a uint64_t as RLP and hash it using Keccak
 * @param [in] data uint64_t to hash
 * @param [out] rlpTmp temporary buffer to store the RLP encoded parameter
 * @param [in] rlpTmpLength size of the temporary buffer to store the RLP encoded parameter
 * @return APDU_NO_RESPONSE if the parameter could be hashed, or an error code
 */
uint16_t hashRLP64(uint64_t data, uint8_t *rlpTmp, uint8_t rlpTmpLength) {
    uint8_t tmp[8];
    uint8_t encodingLength = rlpGetSmallestNumber64EncodingSize(data);
    write_u64_be(tmp, 0, data);
    return hashRLP(tmp + sizeof(tmp) - encodingLength, encodingLength, rlpTmp, rlpTmpLength);
}

static bool handleAuth7702TLV(const uint8_t *payload, uint16_t size) {
    s_auth_7702_ctx auth_7702_ctx = {0};
    s_auth_7702 *auth7702 = &auth_7702_ctx.auth_7702;
    bool parsing_ret = false;
    bool ret = false;
    uint8_t rlpDataSize = 0;
    uint8_t rlpTmp[40];
    uint8_t hashSize;
    uint16_t sw;
    cx_err_t error = CX_INTERNAL_ERROR;
    cx_ecfp_public_key_t publicKey;
    const char *networkName;
    const char *delegateName;

    // Default internal error triggered by CX_CHECK
    g_7702_sw = APDU_RESPONSE_UNKNOWN;

    parsing_ret =
        tlv_parse(payload, size, (f_tlv_data_handler) handle_auth_7702_struct, &auth_7702_ctx);
    if (!parsing_ret || !verify_auth_7702_struct(&auth_7702_ctx)) {
        g_7702_sw = APDU_RESPONSE_INVALID_DATA;
        goto end;
    }

    // Reject if not enabled
    if (!N_storage.eip7702_enable) {
        ui_error_no_7702();
        g_7702_sw = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        goto end;
    }

    // Compute the authorization hash
    // keccak(MAGIC || rlp([chain_id, address, nonce]))
    // * Compute the size of the RLP list data
    rlpDataSize += rlpGetEncodedNumber64Length(auth7702->chainId);
    rlpDataSize += rlpGetEncodedNumberLength(
        auth7702->delegate,
        sizeof(auth7702->delegate));  // 20 bytes RLP string is encoded as a number
    rlpDataSize += rlpGetEncodedNumber64Length(auth7702->nonce);
    // * Hash the components
    rlpTmp[0] = MAGIC_7702;
    hashSize = rlpEncodeListHeader8(rlpDataSize, rlpTmp + 1, sizeof(rlpTmp) - 1);
    if (hashSize == 0) {
        g_7702_sw = APDU_RESPONSE_UNKNOWN;
        goto end;
    }
    if (mem_buffer_allocate((void **) &g_7702_hash_ctx, sizeof(cx_sha3_t)) == false) {
        goto end;
    }
    CX_CHECK(cx_keccak_init_no_throw(g_7702_hash_ctx, 256));
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) g_7702_hash_ctx, 0, rlpTmp, hashSize + 1, NULL, 0));
    sw = hashRLP64(auth7702->chainId, rlpTmp, sizeof(rlpTmp));
    if (sw != APDU_NO_RESPONSE) {
        g_7702_sw = sw;
        goto end;
    }
    sw = hashRLP(auth7702->delegate, sizeof(auth7702->delegate), rlpTmp, sizeof(rlpTmp));
    if (sw != APDU_NO_RESPONSE) {
        g_7702_sw = sw;
        goto end;
    }
    sw = hashRLP64(auth7702->nonce, rlpTmp, sizeof(rlpTmp));
    if (sw != APDU_NO_RESPONSE) {
        g_7702_sw = sw;
        goto end;
    }
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) g_7702_hash_ctx,
                              CX_LAST,
                              NULL,
                              0,
                              tmpCtx.authSigningContext7702.authHash,
                              sizeof(tmpCtx.authSigningContext7702.authHash)));
    // Prepare information to be displayed
    // * Address to be delegated
    strings.common.fromAddress[0] = '0';
    strings.common.fromAddress[1] = 'x';
    CX_CHECK(get_public_key_string(&tmpCtx.authSigningContext7702.bip32,
                                   publicKey.W,
                                   strings.common.fromAddress + 2,
                                   NULL,
                                   auth7702->chainId));
    // * Delegate
    if (!allzeroes(auth7702->delegate, sizeof(auth7702->delegate))) {
        // Check if the delegate is on the whitelist for this chainId
        delegateName = get_delegate_name(&auth7702->chainId, auth7702->delegate);
        if (delegateName == NULL) {
            // Reject if not in the whitelist
            ui_error_no_7702_whitelist();
            g_7702_sw = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
            goto end;
        } else {
            strlcpy(strings.common.toAddress, delegateName, sizeof(strings.common.toAddress));
        }
    }
    // * ChainId
    if (auth7702->chainId == CHAIN_ID_ALL) {
        // handle special wildcard case
        strlcpy(strings.common.network_name, "All", sizeof(strings.common.network_name));
    } else {
        networkName = get_network_name_from_chain_id(&auth7702->chainId);
        if (networkName == NULL) {
            // Display the numeric chainId if no name was found
            if (!u64_to_string(auth7702->chainId,
                               strings.common.network_name,
                               sizeof(strings.common.network_name))) {
                // return APDU_RESPONSE_UNKNOWN;
                // Do not crash if the chain id is too long
                strings.common.network_name[0] = '?';
                strings.common.network_name[1] = '\0';
            }
        } else {
            strlcpy(strings.common.network_name, networkName, sizeof(strings.common.network_name));
        }
    }

    if (allzeroes(auth7702->delegate, sizeof(auth7702->delegate))) {
        ui_sign_7702_revocation();
    } else {
        ui_sign_7702_auth();
    }
    ret = true;

end:
    mem_buffer_cleanup((void **) &g_7702_hash_ctx);
    return ret;
}

uint16_t handleSignEIP7702Authorization(uint8_t p1,
                                        const uint8_t *dataBuffer,
                                        uint8_t dataLength,
                                        unsigned int *flags) {
    g_7702_sw = APDU_RESPONSE_UNKNOWN;
    if (p1 == P1_FIRST_CHUNK) {
        if ((dataBuffer =
                 parseBip32(dataBuffer, &dataLength, &tmpCtx.authSigningContext7702.bip32)) ==
            NULL) {
            return APDU_RESPONSE_INVALID_DATA;
        }
    }
    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, dataLength, dataBuffer, &handleAuth7702TLV)) {
        return g_7702_sw;
    }
    *flags |= IO_ASYNCH_REPLY;
    return APDU_NO_RESPONSE;
}
