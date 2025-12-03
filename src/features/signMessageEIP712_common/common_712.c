#include "apdu_constants.h"
#include "os_io_seproxyhal.h"
#include "crypto_helpers.h"
#include "ui_callbacks.h"
#include "utils.h"
#include "ui_utils.h"
#include "ui_logic.h"
#include "ui_nbgl.h"
#include "cmd_get_tx_simulation.h"

static const uint8_t EIP_712_MAGIC[] = {0x19, 0x01};

void ui_712_approve_cb(void) {
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
    io_seproxyhal_send_status(SWO_SUCCESS, 65, true, false);
}

void ui_712_reject_cb(void) {
    io_seproxyhal_send_status(SWO_CONDITIONS_NOT_SATISFIED, 0, true, false);
}

static char *format_hash(const uint8_t *hash, char *buffer, size_t buffer_size, size_t offset) {
    array_bytes_string(buffer + offset, buffer_size - offset, hash, KECCAK256_HASH_BYTESIZE);
    return buffer + offset;
}

void eip712_format_hash(uint8_t index) {
    if ((g_pairs == NULL) || (g_pairsList == NULL) || (index >= g_pairsList->nbPairs)) {
        return;
    }
    g_pairs[index].item = "Domain hash";
    g_pairs[index].value = format_hash(tmpCtx.messageSigningContext712.domainHash,
                                       strings.tmp.tmp,
                                       sizeof(strings.tmp.tmp),
                                       0);
    index++;
    g_pairs[index].item = "Message hash";
    g_pairs[index].value = format_hash(tmpCtx.messageSigningContext712.messageHash,
                                       strings.tmp.tmp,
                                       sizeof(strings.tmp.tmp),
                                       70);
}

/**
 * Initialize EIP712 flow
 *
 * @param filtering the filtering mode to use for the EIP712 flow
 *
 */
void ui_712_start(e_eip712_filtering_mode filtering) {
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }
    appState = APP_STATE_SIGNING_EIP712;
    memset(&strings, 0, sizeof(strings));
    memset(&warning, 0, sizeof(nbgl_warning_t));
    if (filtering == EIP712_FILTERING_BASIC) {
        // If the user has requested a filtered view, we will not show the warning
        warning.predefinedSet |= SET_BIT(BLIND_SIGNING_WARN);
    }
}
