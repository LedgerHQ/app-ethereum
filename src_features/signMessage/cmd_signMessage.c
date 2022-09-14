#include <stdbool.h>
#include "shared_context.h"
#include "apdu_constants.h"
#include "utils.h"
#include "common_ui.h"

static const char SIGN_MAGIC[] =
    "\x19"
    "Ethereum Signed Message:\n";

/**
 * Check if a given character is a "special" displayable ASCII character
 *
 * @param[in] c character we're checking
 * @return whether the character is special or not
 */
static inline bool is_char_special(char c) {
    return ((c >= '\b') && (c <= '\r'));
}

/**
 * Check if a given data is made of ASCII characters
 *
 * @param[in] data the input data
 * @param[in] the length of the input data
 * @return whether the data is fully ASCII or not
 */
static bool is_data_ascii(const uint8_t *const data, size_t length) {
    for (uint8_t idx = 0; idx < length; ++idx) {
        if (!is_char_special(data[idx]) && ((data[idx] < 0x20) || (data[idx] > 0x7e))) {
            return false;
        }
    }
    return true;
}

/**
 * Initialize value string that will be displayed in the UX STEP
 *
 * @param[in] if the value is ASCII
 */
static void init_value_str(bool is_ascii) {
    if (is_ascii) {
        strings.tmp.tmp[0] = '\0';  // init string as empty
    } else {
        strcpy(strings.tmp.tmp, "0x");  // will display the hex bytes instead
    }
}

/**
 * @return Whether the currently stored data is initialized as ASCII or not
 */
static bool is_value_str_ascii() {
    return (memcmp(strings.tmp.tmp, "0x", 2) != 0);
}

/**
 * Update the global UI string variable by formatting & appending the new data to it
 *
 * @param[in] data the input data
 * @param[in] length the data length
 * @param[in] is_ascii whether the data is ASCII or not
 */
static void feed_value_str(const uint8_t *const data, size_t length, bool is_ascii) {
    uint16_t value_strlen = strlen(strings.tmp.tmp);

    if ((value_strlen + 1) < sizeof(strings.tmp.tmp)) {
        if (is_ascii) {
            uint8_t src_idx = 0;
            uint16_t dst_idx = value_strlen;
            bool prev_is_special = false;

            while ((src_idx < length) && (dst_idx < sizeof(strings.tmp.tmp))) {
                if (prev_is_special) {
                    if (!is_char_special(data[src_idx])) {
                        prev_is_special = false;
                    }
                } else {
                    if (is_char_special(data[src_idx])) {
                        prev_is_special = true;
                        strings.tmp.tmp[dst_idx] = ' ';
                        dst_idx += 1;
                    }
                }
                if (!is_char_special(data[src_idx])) {
                    strings.tmp.tmp[dst_idx] = data[src_idx];
                    dst_idx += 1;
                }
                src_idx += 1;
            }

            if (dst_idx < sizeof(strings.tmp.tmp)) {
                strings.tmp.tmp[dst_idx] = '\0';
            } else {
                const char marker[] = "...";

                memcpy(strings.tmp.tmp + sizeof(strings.tmp.tmp) - sizeof(marker),
                       marker,
                       sizeof(marker));
            }
        } else {
            // truncate to strings.tmp.tmp 's size
            length = MIN(length, (sizeof(strings.tmp.tmp) - value_strlen) / 2);
            for (size_t i = 0; i < length; i++) {
                snprintf(strings.tmp.tmp + value_strlen + 2 * i,
                         sizeof(strings.tmp.tmp) - value_strlen - 2 * i,
                         "%02X",
                         data[i]);
            }
        }
    }
}

void handleSignPersonalMessage(uint8_t p1,
                               uint8_t p2,
                               const uint8_t *workBuffer,
                               uint8_t dataLength,
                               unsigned int *flags,
                               unsigned int *tx) {
    UNUSED(tx);
    uint8_t hashMessage[INT256_LENGTH];

    if (p1 == P1_FIRST) {
        char tmp[11] = {0};

        if (appState != APP_STATE_IDLE) {
            reset_app_context();
        }
        appState = APP_STATE_SIGNING_MESSAGE;

        workBuffer = parseBip32(workBuffer, &dataLength, &tmpCtx.messageSigningContext.bip32);

        if (workBuffer == NULL) {
            THROW(0x6a80);
        }

        if (dataLength < sizeof(uint32_t)) {
            PRINTF("Invalid data\n");
            THROW(0x6a80);
        }

        tmpCtx.messageSigningContext.remainingLength = U4BE(workBuffer, 0);
        workBuffer += sizeof(uint32_t);
        dataLength -= sizeof(uint32_t);
        // Initialize message header + length
        cx_keccak_init(&global_sha3, 256);
        cx_hash((cx_hash_t *) &global_sha3,
                0,
                (uint8_t *) SIGN_MAGIC,
                sizeof(SIGN_MAGIC) - 1,
                NULL,
                0);
        snprintf(tmp, sizeof(tmp), "%u", tmpCtx.messageSigningContext.remainingLength);
        cx_hash((cx_hash_t *) &global_sha3, 0, (uint8_t *) tmp, strlen(tmp), NULL, 0);
        cx_sha256_init(&tmpContent.sha2);

        init_value_str(is_data_ascii(workBuffer, dataLength));

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

    feed_value_str(workBuffer, dataLength, is_value_str_ascii());

    if (tmpCtx.messageSigningContext.remainingLength == 0) {
        cx_hash((cx_hash_t *) &global_sha3,
                CX_LAST,
                workBuffer,
                0,
                tmpCtx.messageSigningContext.hash,
                32);
        cx_hash((cx_hash_t *) &tmpContent.sha2, CX_LAST, workBuffer, 0, hashMessage, 32);

#ifdef NO_CONSENT
        io_seproxyhal_touch_signMessage_ok(NULL);
#else   // NO_CONSENT
        ui_display_sign();
#endif  // NO_CONSENT

        *flags |= IO_ASYNCH_REPLY;

    } else {
        THROW(0x9000);
    }
}
