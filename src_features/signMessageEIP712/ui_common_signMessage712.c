#include "shared_context.h"
#include "ui_callbacks.h"

static const uint8_t EIP_712_MAGIC[] = {0x19, 0x01};

unsigned int io_seproxyhal_touch_signMessage712_v0_ok(__attribute__((unused))
                                                      const bagl_element_t *e) {
    uint8_t privateKeyData[INT256_LENGTH];
    uint8_t hash[INT256_LENGTH];
    uint8_t signature[100];
    cx_ecfp_private_key_t privateKey;
    uint32_t tx = 0;
    io_seproxyhal_io_heartbeat();
    cx_keccak_init(&global_sha3, 256);
    cx_hash((cx_hash_t *) &global_sha3,
            0,
            (uint8_t *) EIP_712_MAGIC,
            sizeof(EIP_712_MAGIC),
            NULL,
            0);
    cx_hash((cx_hash_t *) &global_sha3,
            0,
            tmpCtx.messageSigningContext712.domainHash,
            sizeof(tmpCtx.messageSigningContext712.domainHash),
            NULL,
            0);
    cx_hash((cx_hash_t *) &global_sha3,
            CX_LAST,
            tmpCtx.messageSigningContext712.messageHash,
            sizeof(tmpCtx.messageSigningContext712.messageHash),
            hash,
            sizeof(hash));
    PRINTF("EIP712 hash to sign %.*H\n", 32, hash);
    io_seproxyhal_io_heartbeat();
    os_perso_derive_node_bip32(CX_CURVE_256K1,
                               tmpCtx.messageSigningContext712.bip32Path,
                               tmpCtx.messageSigningContext712.pathLength,
                               privateKeyData,
                               NULL);
    io_seproxyhal_io_heartbeat();
    cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
    explicit_bzero(privateKeyData, sizeof(privateKeyData));
    unsigned int info = 0;
    io_seproxyhal_io_heartbeat();
    cx_ecdsa_sign(&privateKey,
                  CX_RND_RFC6979 | CX_LAST,
                  CX_SHA256,
                  hash,
                  sizeof(hash),
                  signature,
                  sizeof(signature),
                  &info);
    explicit_bzero(&privateKey, sizeof(privateKey));
    G_io_apdu_buffer[0] = 27;
    if (info & CX_ECCINFO_PARITY_ODD) {
        G_io_apdu_buffer[0]++;
    }
    if (info & CX_ECCINFO_xGTn) {
        G_io_apdu_buffer[0] += 2;
    }
    format_signature_out(signature);
    tx = 65;
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    reset_app_context();
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    ui_idle();
    return 0;  // do not redraw the widget
}

unsigned int io_seproxyhal_touch_signMessage712_v0_cancel(__attribute__((unused))
                                                          const bagl_element_t *e) {
    reset_app_context();
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    // Display back the original UX
    ui_idle();
    return 0;  // do not redraw the widget
}
