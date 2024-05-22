#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include "apdu_constants.h"
#include "sign_message.h"
#include "common_ui.h"

static uint8_t processed_size;
static struct {
    sign_message_state sign_state : 1;
    bool ui_started : 1;
} states;

static const char SIGN_MAGIC[] =
    "\x19"
    "Ethereum Signed Message:\n";

/**
 * Send a response APDU with the given Status Word
 *
 * @param[in] sw status word
 */
static void apdu_reply(uint16_t sw) {
    if ((sw != APDU_RESPONSE_OK) && states.ui_started) {
        ui_idle();
    }
    G_io_apdu_buffer[0] = (sw >> 8) & 0xff;
    G_io_apdu_buffer[1] = sw & 0xff;
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

/**
 * Get unprocessed data from last received APDU
 *
 * @return pointer to data in APDU buffer
 */
static const uint8_t *unprocessed_data(void) {
    return &G_io_apdu_buffer[OFFSET_CDATA] + processed_size;
}

/**
 * Get size of unprocessed data from last received APDU
 *
 * @return size of data in bytes
 */
static size_t unprocessed_length(void) {
    return G_io_apdu_buffer[OFFSET_LC] - processed_size;
}

/**
 * Get used space from UI buffer
 *
 * @return size in bytes
 */
static size_t ui_buffer_length(void) {
    return strlen(UI_191_BUFFER);
}

/**
 * Get remaining space from UI buffer
 *
 * @return size in bytes
 */
static size_t remaining_ui_buffer_length(void) {
    // -1 for the ending NULL byte
    return (sizeof(UI_191_BUFFER) - 1) - ui_buffer_length();
}

/**
 * Get free space from UI buffer
 *
 * @return pointer to the free space
 */
static char *remaining_ui_buffer(void) {
    return &UI_191_BUFFER[ui_buffer_length()];
}

/**
 * Reset the UI buffer
 *
 * Simply sets its first byte to a NULL character
 */
static void reset_ui_buffer(void) {
    UI_191_BUFFER[0] = '\0';
}

/**
 * Handle the data specific to the first APDU of an EIP-191 signature
 *
 * @param[in] data the APDU payload
 * @param[in] length the payload size
 * @return pointer to the start of the start of the message; \ref NULL if it failed
 */
static const uint8_t *first_apdu_data(const uint8_t *data, uint8_t *length) {
    cx_err_t error = CX_INTERNAL_ERROR;

    if (appState != APP_STATE_IDLE) {
        apdu_reply(APDU_RESPONSE_CONDITION_NOT_SATISFIED);
    }
    appState = APP_STATE_SIGNING_MESSAGE;
    data = parseBip32(data, length, &tmpCtx.messageSigningContext.bip32);
    if (data == NULL) {
        apdu_reply(APDU_RESPONSE_INVALID_DATA);
        return NULL;
    }

    if (*length < sizeof(uint32_t)) {
        PRINTF("Invalid data\n");
        apdu_reply(APDU_RESPONSE_INVALID_DATA);
        return NULL;
    }

    tmpCtx.messageSigningContext.remainingLength = U4BE(data, 0);
    data += sizeof(uint32_t);
    *length -= sizeof(uint32_t);

    // Initialize message header + length
    CX_CHECK(cx_keccak_init_no_throw(&global_sha3, 256));
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                              0,
                              (uint8_t *) SIGN_MAGIC,
                              sizeof(SIGN_MAGIC) - 1,
                              NULL,
                              0));
    snprintf(strings.tmp.tmp2,
             sizeof(strings.tmp.tmp2),
             "%u",
             tmpCtx.messageSigningContext.remainingLength);
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                              0,
                              (uint8_t *) strings.tmp.tmp2,
                              strlen(strings.tmp.tmp2),
                              NULL,
                              0));
    reset_ui_buffer();
    states.sign_state = STATE_191_HASH_DISPLAY;
    states.ui_started = false;
    return data;
end:
    return NULL;
}

/**
 * Feed the progressive hash with new data
 *
 * @param[in] data the new data
 * @param[in] length the data length
 * @return whether it was successful or not
 */
static bool feed_hash(const uint8_t *const data, const uint8_t length) {
    cx_err_t error = CX_INTERNAL_ERROR;

    if (length > tmpCtx.messageSigningContext.remainingLength) {
        PRINTF("Error: Length mismatch ! (%u > %u)!\n",
               length,
               tmpCtx.messageSigningContext.remainingLength);
        apdu_reply(APDU_RESPONSE_INVALID_DATA);
        return false;
    }
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) &global_sha3, 0, data, length, NULL, 0));
    if ((tmpCtx.messageSigningContext.remainingLength -= length) == 0) {
        // Finalize hash
        CX_CHECK(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                                  CX_LAST,
                                  NULL,
                                  0,
                                  tmpCtx.messageSigningContext.hash,
                                  32));
    }
    return true;
end:
    return false;
}

/**
 * Feed the UI with new data
 */
static void feed_display(void) {
    int c;

    while ((unprocessed_length() > 0) && (remaining_ui_buffer_length() > 0)) {
        c = *(char *) unprocessed_data();
        if (isspace(c))  // to replace all white-space characters as spaces
        {
            c = ' ';
        }
        if (isprint(c)) {
            sprintf(remaining_ui_buffer(), "%c", (char) c);
            processed_size += 1;
        } else {
            if (remaining_ui_buffer_length() >= 4)  // 4 being the fixed length of \x00
            {
                snprintf(remaining_ui_buffer(), remaining_ui_buffer_length(), "\\x%02x", c);
                processed_size += 1;
            } else {
                // fill the rest of the UI buffer spaces, to consider the buffer full
                memset(remaining_ui_buffer(), ' ', remaining_ui_buffer_length());
            }
        }
    }

    if ((remaining_ui_buffer_length() == 0) ||
        (tmpCtx.messageSigningContext.remainingLength == 0)) {
        if (!states.ui_started) {
            ui_191_start();
            states.ui_started = true;
        } else {
            ui_191_switch_to_message();
        }
    }

    if ((unprocessed_length() == 0) && (tmpCtx.messageSigningContext.remainingLength > 0)) {
        apdu_reply(APDU_RESPONSE_OK);
    }
}

/**
 * EIP-191 APDU handler
 *
 * @param[in] p1 instruction parameter 1
 * @param[in] p2 instruction parameter 2
 * @param[in] payload received data
 * @param[in] length data length
 * @return whether the handling of the APDU was successful or not
 */
bool handleSignPersonalMessage(uint8_t p1,
                               uint8_t p2,
                               const uint8_t *const payload,
                               uint8_t length) {
    const uint8_t *data = payload;

    (void) p2;
    processed_size = 0;
    if (p1 == P1_FIRST) {
        if ((data = first_apdu_data(data, &length)) == NULL) {
            return false;
        }
        processed_size = data - payload;
    } else if (p1 != P1_MORE) {
        PRINTF("Error: Unexpected P1 (%u)!\n", p1);
        apdu_reply(APDU_RESPONSE_INVALID_P1_P2);
        return false;
    } else if (appState != APP_STATE_SIGNING_MESSAGE) {
        PRINTF("Error: App not already in signing state!\n");
        apdu_reply(APDU_RESPONSE_INVALID_DATA);
        return false;
    }

    if (!feed_hash(data, length)) {
        return false;
    }

    if (states.sign_state == STATE_191_HASH_DISPLAY) {
        feed_display();
    } else  // hash only
    {
        if (tmpCtx.messageSigningContext.remainingLength == 0) {
            ui_191_switch_to_sign();
        } else {
            apdu_reply(APDU_RESPONSE_OK);
        }
    }
    return true;
}

/**
 * Decide whether to show the question to show more of the message or not
 */
void question_switcher(void) {
    if ((states.sign_state == STATE_191_HASH_DISPLAY) &&
        ((tmpCtx.messageSigningContext.remainingLength > 0) || (unprocessed_length() > 0))) {
        ui_191_switch_to_question();
    } else {
        // Go to Sign / Cancel
        ui_191_switch_to_sign();
    }
}

/**
 * The user has decided to skip the rest of the message
 */
void skip_rest_of_message(void) {
    states.sign_state = STATE_191_HASH_ONLY;
    if (tmpCtx.messageSigningContext.remainingLength > 0) {
        apdu_reply(APDU_RESPONSE_OK);
    } else {
        ui_191_switch_to_sign();
    }
}

/**
 * The user has decided to see the next chunk of the message
 */
void continue_displaying_message(void) {
    reset_ui_buffer();
    if (unprocessed_length() > 0) {
        feed_display();
    }
}
