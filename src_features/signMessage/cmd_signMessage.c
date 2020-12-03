#include "shared_context.h"
#include "apdu_constants.h"
#include "utils.h"
#include "ui_flow.h"

static const char const SIGN_MAGIC[] =
    "\x19"
    "Ethereum Signed Message:\n";

void handleSignPersonalMessage(uint8_t p1,
                               uint8_t p2,
                               uint8_t *workBuffer,
                               uint16_t dataLength,
                               unsigned int *flags,
                               unsigned int *tx) {
    UNUSED(tx);
    uint8_t hashMessage[32];
    if (p1 == P1_FIRST) {
        char tmp[11];
        uint32_t index;
        uint32_t base = 10;
        uint8_t pos = 0;
        uint32_t i;
        if (dataLength < 1) {
            PRINTF("Invalid data\n");
            THROW(0x6a80);
        }
        if (appState != APP_STATE_IDLE) {
            reset_app_context();
        }
        appState = APP_STATE_SIGNING_MESSAGE;
        tmpCtx.messageSigningContext.pathLength = workBuffer[0];
        if ((tmpCtx.messageSigningContext.pathLength < 0x01) ||
            (tmpCtx.messageSigningContext.pathLength > MAX_BIP32_PATH)) {
            PRINTF("Invalid path\n");
            THROW(0x6a80);
        }
        workBuffer++;
        dataLength--;
        for (i = 0; i < tmpCtx.messageSigningContext.pathLength; i++) {
            if (dataLength < 4) {
                PRINTF("Invalid data\n");
                THROW(0x6a80);
            }
            tmpCtx.messageSigningContext.bip32Path[i] = U4BE(workBuffer, 0);
            workBuffer += 4;
            dataLength -= 4;
        }
        if (dataLength < 4) {
            PRINTF("Invalid data\n");
            THROW(0x6a80);
        }
        tmpCtx.messageSigningContext.remainingLength = U4BE(workBuffer, 0);
        workBuffer += 4;
        dataLength -= 4;
        // Initialize message header + length
        cx_keccak_init(&global_sha3, 256);
        cx_hash((cx_hash_t *) &global_sha3,
                0,
                (uint8_t *) SIGN_MAGIC,
                sizeof(SIGN_MAGIC) - 1,
                NULL,
                0);
        for (index = 1; (((index * base) <= tmpCtx.messageSigningContext.remainingLength) &&
                         (((index * base) / base) == index));
             index *= base)
            ;
        for (; index; index /= base) {
            tmp[pos++] = '0' + ((tmpCtx.messageSigningContext.remainingLength / index) % base);
        }
        tmp[pos] = '\0';
        cx_hash((cx_hash_t *) &global_sha3, 0, (uint8_t *) tmp, pos, NULL, 0);
        cx_sha256_init(&tmpContent.sha2);
    } else if (p1 != P1_MORE) {
        THROW(0x6B00);
    }
    if (p2 != 0) {
        THROW(0x6B00);
    }
    if ((p1 == P1_MORE) && (appState != APP_STATE_SIGNING_MESSAGE)) {
        PRINTF("Signature not initialized\n");
        THROW(0x6985);
    }
    if (dataLength > tmpCtx.messageSigningContext.remainingLength) {
        THROW(0x6A80);
    }
    cx_hash((cx_hash_t *) &global_sha3, 0, workBuffer, dataLength, NULL, 0);
    cx_hash((cx_hash_t *) &tmpContent.sha2, 0, workBuffer, dataLength, NULL, 0);
    tmpCtx.messageSigningContext.remainingLength -= dataLength;
    if (tmpCtx.messageSigningContext.remainingLength == 0) {
        cx_hash((cx_hash_t *) &global_sha3,
                CX_LAST,
                workBuffer,
                0,
                tmpCtx.messageSigningContext.hash,
                32);
        cx_hash((cx_hash_t *) &tmpContent.sha2, CX_LAST, workBuffer, 0, hashMessage, 32);
        snprintf(strings.tmp.tmp,
                 sizeof(strings.tmp.tmp),
                 "%.*H",
                 sizeof(hashMessage),
                 hashMessage);

#ifdef NO_CONSENT
        io_seproxyhal_touch_signMessage_ok(NULL);
#else   // NO_CONSENT
        ux_flow_init(0, ux_sign_flow, NULL);
#endif  // NO_CONSENT

        *flags |= IO_ASYNCH_REPLY;

    } else {
        THROW(0x9000);
    }
}
