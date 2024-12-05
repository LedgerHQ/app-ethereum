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

#define SET_IDLE                 \
    do {                         \
        if (states.ui_started) { \
            ui_idle();           \
        }                        \
    } while (0)

/**
 * Send a response APDU with the given Status Word
 *
 * @param[in] sw status word
 */
static void apdu_reply(uint16_t sw) {
    bool idle = (sw != APDU_RESPONSE_OK) && states.ui_started;
    io_seproxyhal_send_status(sw, 0, false, idle);
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
 * @param[out] sw Status Word
 * @return pointer to the start of the message; \ref NULL if it failed
 */
static const uint8_t *first_apdu_data(const uint8_t *data, uint8_t *length, uint16_t *sw) {
    cx_err_t error = CX_INTERNAL_ERROR;

    if (appState != APP_STATE_IDLE) {
        apdu_reply(APDU_RESPONSE_CONDITION_NOT_SATISFIED);
    }
    appState = APP_STATE_SIGNING_MESSAGE;
    data = parseBip32(data, length, &tmpCtx.messageSigningContext.bip32);
    if (data == NULL) {
        *sw = APDU_RESPONSE_INVALID_DATA;
        return NULL;
    }

    if (*length < sizeof(uint32_t)) {
        PRINTF("Invalid data\n");
        *sw = APDU_RESPONSE_INVALID_DATA;
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
    *sw = APDU_RESPONSE_OK;
    return data;
end:
    *sw = error;
    return NULL;
}

/**
 * Feed the progressive hash with new data
 *
 * @param[in] data the new data
 * @param[in] length the data length
 * @return whether it was successful or not
 */
static uint16_t feed_hash(const uint8_t *const data, const uint8_t length) {
    cx_err_t error = CX_INTERNAL_ERROR;

    if (length > tmpCtx.messageSigningContext.remainingLength) {
        PRINTF("Error: Length mismatch ! (%u > %u)!\n",
               length,
               tmpCtx.messageSigningContext.remainingLength);
        SET_IDLE;
        return APDU_RESPONSE_INVALID_DATA;
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
    error = APDU_RESPONSE_OK;
end:
    return error;
}

/**
 * Feed the UI with new data
 */
static uint16_t feed_display(void) {
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
        return APDU_RESPONSE_OK;
    }
    return APDU_NO_RESPONSE;
}

/**
 * EIP-191 APDU handler
 *
 * @param[in] p1 instruction parameter 1
 * @param[in] payload received data
 * @param[in] length data length
 * @param[in] flags io_exchange flag
 * @return whether the handling of the APDU was successful or not
 */
uint16_t handleSignPersonalMessage(uint8_t p1,
                                   const uint8_t *const payload,
                                   uint8_t length,
                                   unsigned int *flags) {
    const uint8_t *data = payload;
    uint16_t sw = APDU_RESPONSE_UNKNOWN;

    processed_size = 0;
    if (p1 == P1_FIRST) {
        if ((data = first_apdu_data(data, &length, &sw)) == NULL) {
            if (sw != APDU_RESPONSE_INVALID_DATA) {
                *flags |= IO_ASYNCH_REPLY;
            }
            return sw;
        }
        processed_size = data - payload;
    } else if (p1 != P1_MORE) {
        PRINTF("Error: Unexpected P1 (%u)!\n", p1);
        SET_IDLE;
        return APDU_RESPONSE_INVALID_P1_P2;
    } else if (appState != APP_STATE_SIGNING_MESSAGE) {
        PRINTF("Error: App not already in signing state!\n");
        SET_IDLE;
        return APDU_RESPONSE_INVALID_DATA;
    }

    sw = feed_hash(data, length);
    if (sw != APDU_RESPONSE_OK) {
        if (sw != APDU_RESPONSE_INVALID_DATA) {
            *flags |= IO_ASYNCH_REPLY;
        }
        return sw;
    }

    if (states.sign_state == STATE_191_HASH_DISPLAY) {
        sw = feed_display();
        if (sw != APDU_RESPONSE_OK) {
            *flags |= IO_ASYNCH_REPLY;
        }
    } else {
        // hash only
        if (tmpCtx.messageSigningContext.remainingLength == 0) {
            ui_191_switch_to_sign();
            sw = APDU_NO_RESPONSE;
            *flags |= IO_ASYNCH_REPLY;
        } else {
            sw = APDU_RESPONSE_OK;
        }
    }
    return sw;
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
    uint16_t sw = APDU_RESPONSE_OK;

    reset_ui_buffer();
    if (unprocessed_length() > 0) {
        sw = feed_display();
    }
    if (sw != APDU_NO_RESPONSE) {
        io_seproxyhal_send_status(sw, 0, sw != APDU_RESPONSE_OK, sw != APDU_RESPONSE_OK);
    }
}
