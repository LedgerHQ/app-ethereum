#include "os_io_seproxyhal.h"
#include "crypto_helpers.h"
#include "common_ui.h"
#include "utils.h"
#include "ui_utils.h"
#include "eip712_v1_ui_logic.h"
#include "ui_nbgl.h"
#include "cmd_get_tx_simulation.h"
#include "app_mem_utils.h"

static const uint8_t EIP_712_MAGIC[] = {0x19, 0x01};

static char *s_domain_hash_str = NULL;
static char *s_message_hash_str = NULL;

void ui_712_approve_cb(void) {
    if (appState != APP_STATE_SIGNING_EIP712) {
        io_seproxyhal_send_status(SWO_CONDITIONS_NOT_SATISFIED, 0, true, false);
        return;
    }
    cx_sha3_t hash_ctx;
    uint8_t hash[INT256_LENGTH];

    io_seproxyhal_io_heartbeat();
    CX_ASSERT(cx_keccak_init_no_throw(&hash_ctx, 256));
    CX_ASSERT(cx_hash_no_throw((cx_hash_t *) &hash_ctx,
                               0,
                               (uint8_t *) EIP_712_MAGIC,
                               sizeof(EIP_712_MAGIC),
                               NULL,
                               0));
    CX_ASSERT(cx_hash_no_throw((cx_hash_t *) &hash_ctx,
                               0,
                               tmpCtx.messageSigningContext712.domainHash,
                               sizeof(tmpCtx.messageSigningContext712.domainHash),
                               NULL,
                               0));
    CX_ASSERT(cx_hash_no_throw((cx_hash_t *) &hash_ctx,
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
                                                  G_io_tx_buffer + 1,
                                                  G_io_tx_buffer + 1 + INT256_LENGTH,
                                                  &info));
    G_io_tx_buffer[0] = ETHEREUM_SIGNATURE_V_BASE;
    if (info & CX_ECCINFO_PARITY_ODD) {
        G_io_tx_buffer[0]++;
    }
    if (info & CX_ECCINFO_xGTn) {
        G_io_tx_buffer[0] += 2;
    }
    io_seproxyhal_send_status(SWO_SUCCESS, ECDSA_SIGNATURE_LENGTH, true, false);
}

void ui_712_reject_cb(void) {
    io_seproxyhal_send_status(SWO_CONDITIONS_NOT_SATISFIED, 0, true, false);
}

void eip712_hash_strs_cleanup(void) {
    APP_MEM_FREE_AND_NULL((void **) &s_domain_hash_str);
    APP_MEM_FREE_AND_NULL((void **) &s_message_hash_str);
}

void eip712_format_hash(uint8_t index) {
    if ((g_pairs == NULL) || (g_pairsList == NULL) || (index + 1 >= g_pairsList->nbPairs)) {
        return;
    }
    const size_t hash_str_size = 2 * CX_KECCAK_256_SIZE + 3;
    if ((s_domain_hash_str == NULL) &&
        !APP_MEM_CALLOC((void **) &s_domain_hash_str, hash_str_size)) {
        return;
    }
    if ((s_message_hash_str == NULL) &&
        !APP_MEM_CALLOC((void **) &s_message_hash_str, hash_str_size)) {
        return;
    }
    messageSigningContext712_t *ctx = &tmpCtx.messageSigningContext712;
    array_bytes_string(s_domain_hash_str, hash_str_size, ctx->domainHash, CX_KECCAK_256_SIZE);
    array_bytes_string(s_message_hash_str, hash_str_size, ctx->messageHash, CX_KECCAK_256_SIZE);
    g_pairs[index].item = "Domain hash";
    g_pairs[index].value = s_domain_hash_str;
    index++;
    g_pairs[index].item = "Message hash";
    g_pairs[index].value = s_message_hash_str;
}

/**
 * Initialize EIP712 flow
 *
 * @param filtering the filtering mode to use for the EIP712 flow
 * @return boolean indicating success or failure
 */
bool ui_712_start(e_eip712_filtering_mode filtering) {
    if (appState != APP_STATE_IDLE && appState != APP_STATE_PREPARING_EIP712) {
        reset_app_context();
    }
    appState = APP_STATE_SIGNING_EIP712;
    memset(&strings, 0, sizeof(strings));
    memset(&warning, 0, sizeof(nbgl_warning_t));
    if (filtering == EIP712_FILTERING_BASIC) {
        // If the user has requested a filtered view, we will not show the warning
        warning.predefinedSet |= SET_BIT(BLIND_SIGNING_WARN);
        warning.predefinedSet |= SET_BIT(GATED_SIGNING_WARN);
    }
    return true;
}
