#include <stdbool.h>
#include "shared_context.h"
#include "apdu_constants.h"
#include "utils.h"
#include "common_ui.h"
#include "sign_message.h"
#include "ui_flow_signMessage.h"

static uint8_t state;
static bool ui_started;
static uint8_t processed_size;
static uint8_t ui_position;

static const char SIGN_MAGIC[] =
    "\x19"
    "Ethereum Signed Message:\n";


const uint8_t *unprocessed_data(void)
{
    return &G_io_apdu_buffer[OFFSET_CDATA] + processed_size;
}

uint8_t unprocessed_length(void)
{
    return G_io_apdu_buffer[OFFSET_LC] - processed_size;
}

uint8_t remaining_ui_length(void)
{
    // -1 for the ending NULL byte
    return (sizeof(strings.tmp.tmp) - 1) - strlen(strings.tmp.tmp);
}

static void switch_to_message(void)
{
    ui_191_switch_to_message();
    ui_position = UI_191_REVIEW;
}

static void switch_to_message_end(void)
{
    ui_191_switch_to_message_end();
    ui_position = UI_191_REVIEW;
}

static void switch_to_sign(void)
{
    ui_191_switch_to_sign();
    ui_position = UI_191_END;
}

static void switch_to_question(void)
{
    ui_191_switch_to_question();
    ui_position = UI_191_QUESTION;
}

const uint8_t *first_apdu_data(const uint8_t *data, uint16_t *length)
{
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }
    appState = APP_STATE_SIGNING_MESSAGE;
    data = parseBip32(data, length, &tmpCtx.messageSigningContext.bip32);
    if (data == NULL) {
        return NULL;
    }

    if (*length < sizeof(uint32_t)) {
        PRINTF("Invalid data\n");
        return NULL;
    }

    tmpCtx.messageSigningContext.remainingLength = U4BE(data, 0);
    data += sizeof(uint32_t);
    *length -= sizeof(uint32_t);

    // Initialize message header + length
    cx_keccak_init(&global_sha3, 256);
    cx_hash((cx_hash_t *) &global_sha3,
            0,
            (uint8_t *) SIGN_MAGIC,
            sizeof(SIGN_MAGIC) - 1,
            NULL,
            0);
    snprintf(strings.tmp.tmp2,
             sizeof(strings.tmp.tmp2),
             "%u",
             tmpCtx.messageSigningContext.remainingLength);
    cx_hash((cx_hash_t *) &global_sha3,
            0,
            (uint8_t *) strings.tmp.tmp2,
            strlen(strings.tmp.tmp2),
            NULL,
            0);
    strings.tmp.tmp[0] = '\0';
    state = STATE_191_HASH_DISPLAY;
    ui_started = false;
    ui_position = UI_191_REVIEW;
    return data;
}

bool feed_hash(const uint8_t *const data, uint8_t length)
{
    if (length > tmpCtx.messageSigningContext.remainingLength)
    {
        PRINTF("Error: Length mismatch ! (%u > %u)!\n",
                length,
                tmpCtx.messageSigningContext.remainingLength);
        return false;
    }
    cx_hash((cx_hash_t *) &global_sha3, 0, data, length, NULL, 0);
    if ((tmpCtx.messageSigningContext.remainingLength -= length) == 0)
    {
        // Finalize hash
        cx_hash((cx_hash_t *) &global_sha3,
                CX_LAST,
                NULL,
                0,
                tmpCtx.messageSigningContext.hash,
                32);
    }
    return true;
}

bool feed_display(void)
{
    uint8_t ui_length;

    while ((unprocessed_length() > 0) && ((ui_length = remaining_ui_length()) > 0))
    {
        sprintf(&strings.tmp.tmp[sizeof(strings.tmp.tmp) - 1 - ui_length], "%c", *(char*)unprocessed_data());
        processed_size += 1;
    }

    if ((remaining_ui_length() == 0) || (tmpCtx.messageSigningContext.remainingLength == 0))
    {
        if (!ui_started)
        {
            ui_display_sign();
            ui_started = true;
        }
        else
        {
            switch_to_message();
        }
    }

    if ((unprocessed_length() == 0) && (tmpCtx.messageSigningContext.remainingLength > 0))
    {
        *(uint16_t *) G_io_apdu_buffer = __builtin_bswap16(0x9000);
        io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    }

    return true;
}

bool handleSignPersonalMessage(uint8_t p1,
                               uint8_t p2,
                               const uint8_t *const payload,
                               uint8_t length)
{
    const uint8_t *data = payload;

    (void)p2;
    processed_size = 0;
    if (p1 == P1_FIRST)
    {
        if ((data = first_apdu_data(data, (uint16_t*)&length)) == NULL)
        {
            return false;
        }
        processed_size = data - payload;
    }
    else if (p1 != P1_MORE)
    {
        PRINTF("Error: Unexpected P1 (%u)!\n", p1);
    }

    if (!feed_hash(data, length))
    {
        return false;
    }

    if (state == STATE_191_HASH_DISPLAY)
    {
        feed_display();
    }
    else // hash only
    {
        if (tmpCtx.messageSigningContext.remainingLength == 0)
        {
            switch_to_sign();
        }
        else
        {
            *(uint16_t *) G_io_apdu_buffer = __builtin_bswap16(0x9000);
            io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
        }
    }
    return true;
}

void dummy_pre_cb(void)
{
    if (ui_position == UI_191_REVIEW)
    {
        if ((state == STATE_191_HASH_DISPLAY) && ((tmpCtx.messageSigningContext.remainingLength > 0) || (unprocessed_length() > 0)))
        {
            switch_to_question();
        }
        else
        {
            // Go to Sign / Cancel
            switch_to_sign();
        }
    }
    else
    {
        ux_flow_prev();
        ui_position = UI_191_REVIEW;
    }
}

void theres_more_click_cb(void)
{
    state = STATE_191_HASH_ONLY;

    if (tmpCtx.messageSigningContext.remainingLength > 0)
    {
        *(uint16_t *) G_io_apdu_buffer = __builtin_bswap16(0x9000);
        io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    }
}

void dummy_post_cb(void)
{
    if (ui_position == UI_191_QUESTION)
    {
        strings.tmp.tmp[0] = '\0'; // empty display string
        processed_size = 0;
        if (unprocessed_length() > 0)
        {
            feed_display();
        }
        // TODO: respond to apdu ?
    }
    else // UI_191_END
    {
        switch_to_message_end();
    }
}
