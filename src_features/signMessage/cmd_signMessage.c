#include <ctype.h>
#include "apdu_constants.h"
#include "sign_message.h"
#include "common_ui.h"
#include "ui_callbacks.h"
#include "ui_utils.h"
#include "mem_utils.h"

typedef struct {
    uint16_t msg_length;
    uint16_t processed_size;
    char *received_buffer;
    char *display_buffer;
} signMsgCtx_t;

cx_sha3_t *g_msg_hash_ctx = NULL;
static signMsgCtx_t *signMsgCtx = NULL;

static const char SIGN_MAGIC[] =
    "\x19"
    "Ethereum Signed Message:\n";

/**
 * Cleanup buffer and go back to main screen
 */
static void set_idle(void) {
    message_cleanup();
    mem_buffer_cleanup((void **) &g_msg_hash_ctx);
    ui_idle();
}

/**
 * Handle the data specific to the first APDU of an EIP-191 signature
 *
 * @param[in,out] data the APDU payload
 * @param[in,out] length the payload size
 * @return whether it was successful or not
 */
static uint16_t first_apdu_data(uint8_t **data, uint8_t *length) {
    cx_err_t error = CX_INTERNAL_ERROR;

    // Initialize the Message Context
    message_cleanup();
    explicit_bzero(&tmpCtx.messageSigningContext, sizeof(tmpCtx.messageSigningContext));

    // Parse the derivation path
    *data = (uint8_t *) parseBip32(*data, length, &tmpCtx.messageSigningContext.bip32);
    if (*data == NULL) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    // Check if the length is valid
    if (*length < sizeof(uint32_t)) {
        PRINTF("Invalid data\n");
        return APDU_RESPONSE_INVALID_DATA;
    }

    if (mem_buffer_allocate((void **) &signMsgCtx, sizeof(signMsgCtx_t)) == false) {
        PRINTF("Memory allocation failed for Sign Context\n");
        return APDU_RESPONSE_INSUFFICIENT_MEMORY;
    }

    // Get the message length
    signMsgCtx->msg_length = U4BE(*data, 0);

    // Allocate the buffer for the message
    if (mem_buffer_allocate((void **) &(signMsgCtx->received_buffer), signMsgCtx->msg_length) ==
        false) {
        PRINTF("Error: Not enough memory!\n");
        return APDU_RESPONSE_INSUFFICIENT_MEMORY;
    }

    // Skip data & length to the message itself
    *data += sizeof(uint32_t);
    *length -= sizeof(uint32_t);

    if (mem_buffer_allocate((void **) &g_msg_hash_ctx, sizeof(cx_sha3_t)) == false) {
        PRINTF("Memory allocation failed for Sign Hash\n");
        return APDU_RESPONSE_INSUFFICIENT_MEMORY;
    }

    // Initialize message header + length hash
    CX_CHECK(cx_keccak_init_no_throw(g_msg_hash_ctx, 256));
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) g_msg_hash_ctx,
                              0,
                              (uint8_t *) SIGN_MAGIC,
                              sizeof(SIGN_MAGIC) - 1,
                              NULL,
                              0));
    snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "%u", signMsgCtx->msg_length);
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) g_msg_hash_ctx,
                              0,
                              (uint8_t *) strings.tmp.tmp,
                              strlen(strings.tmp.tmp),
                              NULL,
                              0));
    error = APDU_RESPONSE_OK;
end:
    return error;
}

/**
 * Process the received data and hash with new data
 *
 * @param[in] data the new data
 * @param[in] length the data length
 * @return whether it was successful or not
 */
static uint16_t process_data(const uint8_t *const data, const uint8_t length) {
    cx_err_t error = CX_INTERNAL_ERROR;

    // Hash the data
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) g_msg_hash_ctx, 0, data, length, NULL, 0));

    // Copy the data to the buffer
    memcpy(&signMsgCtx->received_buffer[signMsgCtx->processed_size], data, length);

    // Decrease the remaining length
    signMsgCtx->processed_size += length;

    error = APDU_RESPONSE_OK;
end:
    return error;
}

/**
 * Finalize the hash computation, and prepare display buffer
 *
 * @return whether it was successful or not
 */
static uint16_t final_process(void) {
    cx_err_t error = CX_INTERNAL_ERROR;
    bool is_hex = false;
    uint16_t buffer_length = 0;
    uint16_t i = 0;
#ifdef SCREEN_SIZE_NANO
    char c;
    uint16_t j = 0;
#endif  // SCREEN_SIZE_NANO

    // Finalize hash
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) g_msg_hash_ctx,
                              CX_LAST,
                              NULL,
                              0,
                              tmpCtx.messageSigningContext.hash,
                              32));

    // Display buffer length
    buffer_length = signMsgCtx->msg_length;

    // Determine if the buffer is Ascii or hex
    for (i = 0; i < signMsgCtx->msg_length; i++) {
        if (!isprint((int) signMsgCtx->received_buffer[i]) &&
            !isspace((int) signMsgCtx->received_buffer[i])) {
            // Hexadecimal message
            is_hex = true;
            buffer_length *= 2;  // To convert hex byte to char
            buffer_length += 2;  // for "0x"
            break;
        }
    }
    // Allocate the buffer for the display
    buffer_length++;  // for the NULL byte
    if (mem_buffer_allocate((void **) &(signMsgCtx->display_buffer), buffer_length) == false) {
        PRINTF("Error: Not enough memory!\n");
        error = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        goto end;
    }

    if (is_hex) {
        // Copy the "0x" prefix
        memcpy(signMsgCtx->display_buffer, "0x", 2);
        // Convert the message to ascii
        if (bytes_to_lowercase_hex(signMsgCtx->display_buffer + 2,
                                   buffer_length - 2,
                                   (const uint8_t *) signMsgCtx->received_buffer,
                                   signMsgCtx->msg_length) < 0) {
            // SHould never happen, buffer is large enough
            PRINTF("Error: Not enough memory!\n");
            error = APDU_RESPONSE_INSUFFICIENT_MEMORY;
            goto end;
        }
    } else {
#ifdef SCREEN_SIZE_NANO
        for (i = 0; i < signMsgCtx->msg_length; i++) {
            c = signMsgCtx->received_buffer[i];
            if (isspace((int) c)) {
                // to replace all white-space characters as spaces
                c = ' ';
            }
            signMsgCtx->display_buffer[j++] = c;
        }
#else   // SCREEN_SIZE_NANO
        // Copy the message to the display buffer
        memcpy(signMsgCtx->display_buffer, signMsgCtx->received_buffer, signMsgCtx->msg_length);
#endif  // SCREEN_SIZE_NANO
    }

    error = APDU_RESPONSE_OK;
end:
    mem_buffer_cleanup((void **) &g_msg_hash_ctx);
    return error;
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
    uint8_t *data = (uint8_t *) payload;
    uint16_t sw = APDU_RESPONSE_UNKNOWN;

    if (p1 == P1_FIRST) {
        // Check if the app is in idle state
        if (appState != APP_STATE_IDLE) {
            set_idle();
            return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        }
        appState = APP_STATE_SIGNING_MESSAGE;

        sw = first_apdu_data(&data, &length);
        if (sw != APDU_RESPONSE_OK) {
            return sw;
        }
    } else if (p1 != P1_MORE) {
        PRINTF("Error: Unexpected P1 (%u)!\n", p1);
        set_idle();
        return APDU_RESPONSE_INVALID_P1_P2;
    } else if (appState != APP_STATE_SIGNING_MESSAGE) {
        PRINTF("Error: App not already in signing state!\n");
        set_idle();
        return APDU_RESPONSE_INVALID_DATA;
    }

    // Check if the received chunk data is too long
    if ((length + signMsgCtx->processed_size) > signMsgCtx->msg_length) {
        PRINTF("Error: Length mismatch ! (%u > %u)!\n",
               (length + signMsgCtx->processed_size),
               signMsgCtx->msg_length);
        set_idle();
        return APDU_RESPONSE_INVALID_DATA;
    }

    // Hash the data and copy the data to the buffer
    sw = process_data(data, length);
    if (sw != APDU_RESPONSE_OK) {
        return sw;
    }

    // Start the UI if the buffer is fully received
    if (signMsgCtx->processed_size == signMsgCtx->msg_length) {
        sw = final_process();
        if (sw != APDU_RESPONSE_OK) {
            set_idle();
            return sw;
        }
        ui_191_start(signMsgCtx->display_buffer);
        *flags |= IO_ASYNCH_REPLY;
        return APDU_NO_RESPONSE;
    }
    return APDU_RESPONSE_OK;
}

/**
 * Cleanup buffer
 */
void message_cleanup(void) {
    if (signMsgCtx != NULL) {
        mem_buffer_cleanup((void **) &signMsgCtx->received_buffer);
        mem_buffer_cleanup((void **) &signMsgCtx->display_buffer);
    }
    mem_buffer_cleanup((void **) &signMsgCtx);
}
