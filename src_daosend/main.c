/*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "os.h"
#include "cx.h"
#include <stdbool.h>
#include "app_cx_sha3.h"
#include "ethUstream.h"
#include "ethUtils.h"
#include "uint256.h"

#include "os_io_seproxyhal.h"
#include "string.h"
unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

unsigned char usb_enable_request; // request to turn on USB
unsigned char uiDone; // notification to come back to the APDU event loop
unsigned int
    currentUiElement; // currently displayed UI element in a set of elements
unsigned char
    element_displayed; // notification of something displayed in a touch handler

// UI currently displayed
enum UI_STATE { UI_IDLE, UI_ADDRESS, UI_APPROVAL };

enum UI_STATE uiState;

void io_usb_enable(unsigned char enabled);
void os_boot(void);

unsigned int io_seproxyhal_touch_exit(bagl_element_t *e);
unsigned int io_seproxyhal_touch_tx_ok(bagl_element_t *e);
unsigned int io_seproxyhal_touch_tx_cancel(bagl_element_t *e);
unsigned int io_seproxyhal_touch_address_ok(bagl_element_t *e);
unsigned int io_seproxyhal_touch_address_cancel(bagl_element_t *e);

uint32_t set_result_get_publicKey(void);

#define MAX_BIP32_PATH 10

#define CLA 0xE0
#define INS_GET_PUBLIC_KEY 0x02
#define INS_SIGN 0x04
#define INS_ADD_SELF 0x06
#define P1_CONFIRM 0x01
#define P1_NON_CONFIRM 0x00
#define P1_FIRST 0x00
#define P1_MORE 0x80

#define OFFSET_CLA 0
#define OFFSET_INS 1
#define OFFSET_P1 2
#define OFFSET_P2 3
#define OFFSET_LC 4
#define OFFSET_CDATA 5

#define DAO_TOKENS_UNIT 16

typedef struct publicKeyContext_t {
    cx_ecfp_public_key_t publicKey;
    uint8_t address[41];
} publicKeyContext_t;

typedef struct transactionContext_t {
    uint8_t pathLength;
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t hash[32];
} transactionContext_t;

typedef struct daosendContext_t {
    uint8_t data[4 + 32 + 32];
    uint32_t dataFieldPos;
    uint8_t selfAddress[20];
    bool selfAddressProvided;
} daosendContext_t;

char lineBuffer[50];
union {
    publicKeyContext_t publicKeyContext;
    transactionContext_t transactionContext;
} tmpCtx;
txContext_t txContext;
txContent_t txContent;
daosendContext_t daoContext;
app_cx_sha3_t sha3;
volatile char addressSummary[21];
volatile char address1[21];
volatile char address2[21];
volatile char fullAmount[50];

static const uint8_t const DAO_TRANSFER_ID[] = {0xa9, 0x05, 0x9c, 0xbb};
static const uint8_t const DAO_ADDRESS[] = {
    0xbb, 0x9b, 0xc2, 0x44, 0xd7, 0x98, 0x12, 0x3f, 0xde, 0x78,
    0x3f, 0xcc, 0x1c, 0x72, 0xd3, 0xbb, 0x8c, 0x18, 0x94, 0x13};

// blank the screen
static const bagl_element_t const bagl_ui_erase_all[] = {
    {{BAGL_RECTANGLE, 0x00, 0, 60, 320, 420, 0, 0, BAGL_FILL, 0xf9f9f9,
      0xf9f9f9, 0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
};

bagl_element_t const bagl_ui_address[] = {
    // type                                 id    x    y    w    h    s  r  fill
    // fg        bg        font icon   text, out, over, touch
    {{BAGL_RECTANGLE, 0x00, 0, 0, 320, 60, 0, 0, BAGL_FILL, 0x1d2028, 0x1d2028,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABEL, 0x00, 20, 0, 320, 60, 0, 0, BAGL_FILL, 0xFFFFFF, 0x1d2028,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_MIDDLE, 0},
     "TheDAO send token",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_BUTTON | BAGL_FLAG_TOUCHABLE, 0x00, 35, 385, 120, 40, 0, 6,
      BAGL_FILL, 0xcccccc, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER |
          BAGL_FONT_ALIGNMENT_MIDDLE,
      0},
     "CANCEL",
     0,
     0x37ae99,
     0xF9F9F9,
     (bagl_element_callback_t)io_seproxyhal_touch_address_cancel,
     NULL,
     NULL},
    {{BAGL_BUTTON | BAGL_FLAG_TOUCHABLE, 0x00, 165, 385, 120, 40, 0, 6,
      BAGL_FILL, 0x41ccb4, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER |
          BAGL_FONT_ALIGNMENT_MIDDLE,
      0},
     "CONFIRM",
     0,
     0x37ae99,
     0xF9F9F9,
     (bagl_element_callback_t)io_seproxyhal_touch_address_ok,
     NULL,
     NULL},

    {{BAGL_LABEL, 0x00, 0, 147, 320, 32, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Confirm address",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABEL, 0x00, 0, 280, 320, 33, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_16px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     (const char *)addressSummary,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABEL, 0x00, 0, 310, 320, 33, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     (const char *)address1,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABEL, 0x00, 0, 330, 320, 33, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     (const char *)address2,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
};

// UI to approve or deny the signature proposal
static const bagl_element_t const bagl_ui_approval[] = {

    // type                                 id    x    y    w    h    s  r  fill
    // fg        bg        font icon   text, out, over, touch
    {{BAGL_RECTANGLE, 0x00, 0, 0, 320, 60, 0, 0, BAGL_FILL, 0x1d2028, 0x1d2028,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABEL, 0x00, 20, 0, 320, 60, 0, 0, BAGL_FILL, 0xFFFFFF, 0x1d2028,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_MIDDLE, 0},
     "TheDAO send token",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_BUTTON | BAGL_FLAG_TOUCHABLE, 0x00, 35, 385, 120, 40, 0, 6,
      BAGL_FILL, 0xcccccc, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER |
          BAGL_FONT_ALIGNMENT_MIDDLE,
      0},
     "CANCEL",
     0,
     0x37ae99,
     0xF9F9F9,
     (bagl_element_callback_t)io_seproxyhal_touch_tx_cancel,
     NULL,
     NULL},
    {{BAGL_BUTTON | BAGL_FLAG_TOUCHABLE, 0x00, 165, 385, 120, 40, 0, 6,
      BAGL_FILL, 0x41ccb4, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER |
          BAGL_FONT_ALIGNMENT_MIDDLE,
      0},
     "CONFIRM",
     0,
     0x37ae99,
     0xF9F9F9,
     (bagl_element_callback_t)io_seproxyhal_touch_tx_ok,
     NULL,
     NULL},

    {{BAGL_LABEL, 0x00, 0, 87, 320, 32, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "CONFIRM THEDAO TOKENS SEND",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABEL, 0x00, 0, 125, 320, 33, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_16px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     (const char *)fullAmount,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABEL, 0x00, 0, 175, 320, 33, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_16px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     (const char *)addressSummary,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABEL, 0x00, 0, 205, 320, 33, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     (const char *)address1,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABEL, 0x00, 0, 225, 320, 33, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     (const char *)address2,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

};

// UI displayed when no signature proposal has been received
static const bagl_element_t const bagl_ui_idle[] = {

    {{BAGL_RECTANGLE, 0x00, 0, 0, 320, 60, 0, 0, BAGL_FILL, 0x1d2028, 0x1d2028,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABEL, 0x00, 20, 0, 320, 60, 0, 0, BAGL_FILL, 0xFFFFFF, 0x1d2028,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_MIDDLE, 0},
     "TheDAO send token",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_BUTTON | BAGL_FLAG_TOUCHABLE, 0x00, 190, 215, 120, 40, 0, 6,
      BAGL_FILL, 0x41ccb4, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER |
          BAGL_FONT_ALIGNMENT_MIDDLE,
      0},
     "Exit",
     0,
     0x37ae99,
     0xF9F9F9,
     (bagl_element_callback_t)io_seproxyhal_touch_exit,
     NULL,
     NULL}

};

// TODO : replace with 1.2 SDK
int app_cx_ecfp_init_private_key(cx_curve_t curve, unsigned char WIDE *rawkey,
                                 int key_len, cx_ecfp_private_key_t *key) {
    key->curve = curve;
    key->d_len = key_len;
    if (rawkey != NULL) {
        os_memmove(key->d, rawkey, key_len);
    }
    return key_len;
}

unsigned int io_seproxyhal_touch_exit(bagl_element_t *e) {
    // Go back to the dashboard
    os_sched_exit(0);
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_address_ok(bagl_element_t *e) {
    uint32_t tx = set_result_get_publicKey();
    uiDone = 1;
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    uiState = UI_IDLE;
    currentUiElement = 0;
    io_seproxyhal_display(&bagl_ui_erase_all[0]);
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_address_cancel(bagl_element_t *e) {
    uiDone = 1;
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    // Display back the original UX
    uiState = UI_IDLE;
    currentUiElement = 0;
    io_seproxyhal_display(&bagl_ui_erase_all[0]);
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_tx_ok(bagl_element_t *e) {
    uint8_t privateKeyData[32];
    uint8_t signature[100];
    uint8_t signatureLength;
    cx_ecfp_private_key_t privateKey;
    uint32_t tx = 0;
    uint8_t rLength, sLength, rOffset, sOffset;
    os_perso_derive_seed_bip32(tmpCtx.transactionContext.bip32Path,
                               tmpCtx.transactionContext.pathLength,
                               privateKeyData, NULL);
    app_cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32,
                                 &privateKey);
    os_memset(privateKeyData, 0, sizeof(privateKeyData));
    signatureLength =
        cx_ecdsa_sign(&privateKey, CX_RND_RFC6979 | CX_LAST, CX_SHA256,
                      tmpCtx.transactionContext.hash,
                      sizeof(tmpCtx.transactionContext.hash), signature);
    os_memset(&privateKey, 0, sizeof(privateKey));
    // Parity is present in the sequence tag in the legacy API
    G_io_apdu_buffer[0] = 27 + (signature[0] & 0x01);
    rLength = signature[3];
    sLength = signature[4 + rLength + 1];
    rOffset = (rLength == 33 ? 1 : 0);
    sOffset = (sLength == 33 ? 1 : 0);
    os_memmove(G_io_apdu_buffer + 1, signature + 4 + rOffset, 32);
    os_memmove(G_io_apdu_buffer + 1 + 32, signature + 4 + rLength + 2 + sOffset,
               32);
    tx = 65;
    uiDone = 1;
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    uiState = UI_IDLE;
    currentUiElement = 0;
    io_seproxyhal_display(&bagl_ui_erase_all[0]);
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_tx_cancel(bagl_element_t *e) {
    uiDone = 1;
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    // Display back the original UX
    uiState = UI_IDLE;
    currentUiElement = 0;
    io_seproxyhal_display(&bagl_ui_erase_all[0]);
    return 0; // do not redraw the widget
}

void reset(void) {
}

unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;

    // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
    case CHANNEL_SPI:
        if (tx_len) {
            io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

            if (channel & IO_RESET_AFTER_REPLIED) {
                reset();
            }
            return 0; // nothing received from the master so far (it's a tx
                      // transaction)
        } else {
            return io_seproxyhal_spi_recv(G_io_apdu_buffer,
                                          sizeof(G_io_apdu_buffer), 0);
        }

    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

uint32_t set_result_get_publicKey() {
    uint32_t tx = 0;
    G_io_apdu_buffer[tx++] = 65;
    os_memmove(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.publicKey.W, 65);
    tx += 65;
    G_io_apdu_buffer[tx++] = 40;
    os_memmove(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.address, 40);
    tx += 40;
    return tx;
}

void convertUint256BE(uint8_t *data, uint32_t length, uint256_t *target) {
    uint8_t tmp[32];
    os_memset(tmp, 0, 32);
    os_memmove(tmp + 32 - length, data, length);
    readu256BE(tmp, target);
}

bool customProcessor(txContext_t *context) {
    if (context->currentField != TX_RLP_DATA) {
        return false;
    }
    if (context->currentFieldLength != sizeof(daoContext.data)) {
        screen_printf("Invalid length for RLP_DATA\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, daoContext.data + context->currentFieldPos,
                   copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }

    return true;
}

void sample_main(void) {
    volatile unsigned int rx = 0;
    volatile unsigned int tx = 0;
    volatile unsigned int flags = 0;

    // DESIGN NOTE: the bootloader ignores the way APDU are fetched. The only
    // goal is to retrieve APDU.
    // When APDU are to be fetched from multiple IOs, like NFC+USB+BLE, make
    // sure the io_event is called with a
    // switch event, before the apdu is replied to the bootloader. This avoid
    // APDU injection faults.
    for (;;) {
        volatile unsigned short sw = 0;

        BEGIN_TRY {
            TRY {
                rx = tx;
                tx = 0; // ensure no race in catch_other if io_exchange throws
                        // an error
                rx = io_exchange(CHANNEL_APDU | flags, rx);

                // no apdu received, well, reset the session, and reset the
                // bootloader configuration
                if (rx == 0) {
                    THROW(0x6982);
                }

                if (G_io_apdu_buffer[OFFSET_CLA] != CLA) {
                    THROW(0x6E00);
                }

                switch (G_io_apdu_buffer[OFFSET_INS]) {
                case INS_GET_PUBLIC_KEY: {
                    uint8_t privateKeyData[32];
                    uint32_t bip32Path[MAX_BIP32_PATH];
                    uint32_t i;
                    uint8_t bip32PathLength = G_io_apdu_buffer[OFFSET_CDATA];
                    uint8_t p1 = G_io_apdu_buffer[OFFSET_P1];
                    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA + 1;
                    cx_ecfp_private_key_t privateKey;

                    if ((bip32PathLength < 0x01) ||
                        (bip32PathLength > MAX_BIP32_PATH)) {
                        screen_printf("Invalid path\n");
                        THROW(0x6a80);
                    }
                    if ((p1 != P1_CONFIRM) && (p1 != P1_NON_CONFIRM)) {
                        THROW(0x6B00);
                    }
                    if (G_io_apdu_buffer[OFFSET_P2] != 0) {
                        THROW(0x6B00);
                    }
                    for (i = 0; i < bip32PathLength; i++) {
                        bip32Path[i] = (dataBuffer[0] << 24) |
                                       (dataBuffer[1] << 16) |
                                       (dataBuffer[2] << 8) | (dataBuffer[3]);
                        dataBuffer += 4;
                    }
                    os_perso_derive_seed_bip32(bip32Path, bip32PathLength,
                                               privateKeyData, NULL);
                    app_cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData,
                                                 32, &privateKey);
                    cx_ecfp_generate_pair(CX_CURVE_256K1,
                                          &tmpCtx.publicKeyContext.publicKey,
                                          &privateKey, 1);
                    os_memset(&privateKey, 0, sizeof(privateKey));
                    os_memset(privateKeyData, 0, sizeof(privateKeyData));
                    getEthAddressStringFromKey(
                        &tmpCtx.publicKeyContext.publicKey,
                        tmpCtx.publicKeyContext.address, &sha3);
                    if (p1 == P1_NON_CONFIRM) {
                        tx = set_result_get_publicKey();
                        THROW(0x9000);
                    } else {
                        os_memmove((unsigned char *)addressSummary,
                                   tmpCtx.publicKeyContext.address, 5);
                        os_memmove((unsigned char *)(addressSummary + 5),
                                   " ... ", 5);
                        os_memmove((unsigned char *)(addressSummary + 10),
                                   tmpCtx.publicKeyContext.address + 40 - 5, 5);
                        addressSummary[15] = '\0';
                        os_memmove((unsigned char *)address1,
                                   tmpCtx.publicKeyContext.address, 20);
                        address1[20] = '\0';
                        os_memmove((unsigned char *)address2,
                                   tmpCtx.publicKeyContext.address + 20, 20);
                        address2[20] = '\0';
                        uiState = UI_ADDRESS;
                        currentUiElement = 0;
                        uiDone = 0;
                        io_seproxyhal_display(&bagl_ui_erase_all[0]);
                        // Pump SPI events until the UI has been displayed, then
                        // go
                        // back to the original event loop
                        while (!uiDone) {
                            unsigned int rx_len;
                            rx_len = io_seproxyhal_spi_recv(
                                G_io_seproxyhal_spi_buffer,
                                sizeof(G_io_seproxyhal_spi_buffer), 0);
                            if (io_seproxyhal_handle_event()) {
                                io_seproxyhal_general_status();
                                continue;
                            }
                            io_event(CHANNEL_SPI);
                        }
                        continue;
                    }
                }

                break;

                case INS_ADD_SELF: {
                    uint8_t privateKeyData[32];
                    uint32_t bip32Path[MAX_BIP32_PATH];
                    uint32_t i;
                    uint8_t bip32PathLength = G_io_apdu_buffer[OFFSET_CDATA];
                    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA + 1;
                    cx_ecfp_private_key_t privateKey;

                    if ((bip32PathLength < 0x01) ||
                        (bip32PathLength > MAX_BIP32_PATH)) {
                        screen_printf("Invalid path\n");
                        THROW(0x6a80);
                    }
                    if ((G_io_apdu_buffer[OFFSET_P1] != 0) ||
                        (G_io_apdu_buffer[OFFSET_P2] != 0)) {
                        THROW(0x6B00);
                    }
                    for (i = 0; i < bip32PathLength; i++) {
                        bip32Path[i] = (dataBuffer[0] << 24) |
                                       (dataBuffer[1] << 16) |
                                       (dataBuffer[2] << 8) | (dataBuffer[3]);
                        dataBuffer += 4;
                    }
                    os_perso_derive_seed_bip32(bip32Path, bip32PathLength,
                                               privateKeyData, NULL);
                    app_cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData,
                                                 32, &privateKey);
                    cx_ecfp_generate_pair(CX_CURVE_256K1,
                                          &tmpCtx.publicKeyContext.publicKey,
                                          &privateKey, 1);
                    os_memset(&privateKey, 0, sizeof(privateKey));
                    os_memset(privateKeyData, 0, sizeof(privateKeyData));
                    getEthAddressFromKey(&tmpCtx.publicKeyContext.publicKey,
                                         daoContext.selfAddress, &sha3);
                    daoContext.selfAddressProvided = true;
                    THROW(0x9000);
                } break;

                case INS_SIGN: {
                    uint8_t commandLength = G_io_apdu_buffer[OFFSET_LC];
                    uint8_t *workBuffer = G_io_apdu_buffer + OFFSET_CDATA;
                    parserStatus_e txResult;
                    uint256_t uint256;
                    uint32_t i;
                    uint8_t address[41];
                    if (G_io_apdu_buffer[OFFSET_P1] == P1_FIRST) {
                        tmpCtx.transactionContext.pathLength = workBuffer[0];
                        if ((tmpCtx.transactionContext.pathLength < 0x01) ||
                            (tmpCtx.transactionContext.pathLength >
                             MAX_BIP32_PATH)) {
                            screen_printf("Invalid path\n");
                            THROW(0x6a80);
                        }
                        workBuffer++;
                        commandLength--;
                        for (i = 0; i < tmpCtx.transactionContext.pathLength;
                             i++) {
                            tmpCtx.transactionContext.bip32Path[i] =
                                (workBuffer[0] << 24) | (workBuffer[1] << 16) |
                                (workBuffer[2] << 8) | (workBuffer[3]);
                            workBuffer += 4;
                            commandLength -= 4;
                        }
                        initTx(&txContext, &sha3, &txContent, customProcessor,
                               NULL);
                    } else if (G_io_apdu_buffer[OFFSET_P1] != P1_MORE) {
                        THROW(0x6B00);
                    }
                    if (G_io_apdu_buffer[OFFSET_P2] != 0) {
                        THROW(0x6B00);
                    }
                    if (txContext.currentField == TX_RLP_NONE) {
                        screen_printf("Parser not initialized\n");
                        THROW(0x6985);
                    }
                    txResult = processTx(&txContext, workBuffer, commandLength);
                    switch (txResult) {
                    case USTREAM_FINISHED:
                        break;
                    case USTREAM_PROCESSING:
                        THROW(0x9000);
                    case USTREAM_FAULT:
                        THROW(0x6A80);
                    default:
                        screen_printf("Unexpected parser status\n");
                        THROW(0x6A80);
                    }

                    // Store the hash
                    app_cx_hash((cx_hash_t *)&sha3, CX_LAST,
                                tmpCtx.transactionContext.hash, 0,
                                tmpCtx.transactionContext.hash);
                    // Abort if not sending to the DAO
                    if (os_memcmp(txContent.destination, DAO_ADDRESS, 20) !=
                        0) {
                        screen_printf("Invalid target address\n");
                        THROW(0x6A80);
                    }
                    // Abort if sending an amount
                    if (txContent.value.length != 0) {
                        screen_printf("Invalid amount\n");
                        THROW(0x6A80);
                    }
                    // Abort if not calling the transfer function
                    if (os_memcmp(daoContext.data, DAO_TRANSFER_ID, 4) != 0) {
                        screen_printf("Invalid contract call ID\n");
                        THROW(0x6A80);
                    }
                    if (daoContext.selfAddressProvided &&
                        os_memcmp(daoContext.selfAddress,
                                  daoContext.data + 4 + 12, 20) == 0) {
                        os_memmove((unsigned char *)addressSummary, "Self", 5);
                        address1[0] = '\0';
                        address2[0] = '\0';
                    } else {
                        // Add address
                        getEthAddressStringFromBinary(daoContext.data + 4 + 12,
                                                      address, &sha3);
                        os_memmove((unsigned char *)addressSummary, address, 5);
                        os_memmove((unsigned char *)(addressSummary + 5),
                                   " ... ", 5);
                        os_memmove((unsigned char *)(addressSummary + 10),
                                   address + 40 - 5, 5);
                        addressSummary[15] = '\0';
                        os_memmove((unsigned char *)address1, address, 20);
                        address1[20] = '\0';
                        os_memmove((unsigned char *)address2, address + 20, 20);
                        address2[20] = '\0';
                    }
                    // Add token amount
                    convertUint256BE(daoContext.data + 4 + 32, 32, &uint256);
                    tostring256(&uint256, 10, (char *)(G_io_apdu_buffer + 100),
                                100);
                    i = 0;
                    while (G_io_apdu_buffer[100 + i]) {
                        i++;
                    }
                    adjustDecimals((char *)(G_io_apdu_buffer + 100), i,
                                   (char *)G_io_apdu_buffer, 100,
                                   DAO_TOKENS_UNIT);
                    i = 0;
                    fullAmount[0] = 'D';
                    fullAmount[1] = 'A';
                    fullAmount[2] = 'O';
                    fullAmount[3] = ' ';
                    while (G_io_apdu_buffer[i]) {
                        fullAmount[4 + i] = G_io_apdu_buffer[i];
                        i++;
                    }
                    fullAmount[4 + i] = '\0';

                    uiState = UI_APPROVAL;
                    currentUiElement = 0;
                    uiDone = 0;
                    io_seproxyhal_display(&bagl_ui_erase_all[0]);
                    // Pump SPI events until the UI has been displayed, then go
                    // back to the original event loop
                    while (!uiDone) {
                        unsigned int rx_len;
                        rx_len = io_seproxyhal_spi_recv(
                            G_io_seproxyhal_spi_buffer,
                            sizeof(G_io_seproxyhal_spi_buffer), 0);
                        if (io_seproxyhal_handle_event()) {
                            io_seproxyhal_general_status();
                            continue;
                        }
                        io_event(CHANNEL_SPI);
                    }
                    continue;
                } break;

                case 0xFF: // return to dashboard
                    goto return_to_dashboard;

                default:
                    THROW(0x6D00);
                    break;
                }
            }
            CATCH_OTHER(e) {
                switch (e & 0xF000) {
                case 0x6000:
                    // Wipe the transaction context and report the exception
                    sw = e;
                    os_memset(&txContext, 0, sizeof(txContext));
                    break;
                case 0x9000:
                    // All is well
                    sw = e;
                    break;
                default:
                    // Internal error
                    sw = 0x6800 | (e & 0x7FF);
                    break;
                }
                // Unexpected exception => report
                G_io_apdu_buffer[tx] = sw >> 8;
                G_io_apdu_buffer[tx + 1] = sw;
                tx += 2;
            }
            FINALLY {
            }
        }
        END_TRY;
    }

return_to_dashboard:
    return;
}

void io_seproxyhal_display(const bagl_element_t *element) {
    element_displayed = 1;
    return io_seproxyhal_display_default((bagl_element_t *)element);
}

unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed
    unsigned int offset = 0;

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_FINGER_EVENT: {
        bagl_element_t *elements = NULL;
        unsigned int elementsSize = 0;
        if (uiState == UI_IDLE) {
            elements = (bagl_element_t *)bagl_ui_idle;
            elementsSize = sizeof(bagl_ui_idle) / sizeof(bagl_element_t);
        } else if (uiState == UI_ADDRESS) {
            elements = (bagl_element_t *)bagl_ui_address;
            elementsSize = sizeof(bagl_ui_address) / sizeof(bagl_element_t);
        } else if (uiState == UI_APPROVAL) {
            elements = (bagl_element_t *)bagl_ui_approval;
            elementsSize = sizeof(bagl_ui_approval) / sizeof(bagl_element_t);
        }
        if (elements != NULL) {
            io_seproxyhal_touch(elements, elementsSize,
                                (G_io_seproxyhal_spi_buffer[4] << 8) |
                                    (G_io_seproxyhal_spi_buffer[5] & 0xFF),
                                (G_io_seproxyhal_spi_buffer[6] << 8) |
                                    (G_io_seproxyhal_spi_buffer[7] & 0xFF),
                                G_io_seproxyhal_spi_buffer[3]);
            if (!element_displayed) {
                goto general_status;
            }
        } else {
            goto general_status;
        }
    } break;

#ifdef HAVE_BLE
    // Make automatically discoverable again when disconnected

    case SEPROXYHAL_TAG_BLE_CONNECTION_EVENT:
        if (G_io_seproxyhal_spi_buffer[3] == 0) {
            // TODO : cleaner reset sequence
            // first disable BLE before turning it off
            G_io_seproxyhal_spi_buffer[0] = SEPROXYHAL_TAG_BLE_RADIO_POWER;
            G_io_seproxyhal_spi_buffer[1] = 0;
            G_io_seproxyhal_spi_buffer[2] = 1;
            G_io_seproxyhal_spi_buffer[3] = 0;
            io_seproxyhal_spi_send(G_io_seproxyhal_spi_buffer, 4);
            // send BLE power on (default parameters)
            G_io_seproxyhal_spi_buffer[0] = SEPROXYHAL_TAG_BLE_RADIO_POWER;
            G_io_seproxyhal_spi_buffer[1] = 0;
            G_io_seproxyhal_spi_buffer[2] = 1;
            G_io_seproxyhal_spi_buffer[3] = 3; // ble on & advertise
            io_seproxyhal_spi_send(G_io_seproxyhal_spi_buffer, 5);
        }
        goto general_status;
#endif

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        if (uiState == UI_IDLE) {
            if (currentUiElement <
                (sizeof(bagl_ui_idle) / sizeof(bagl_element_t))) {
                io_seproxyhal_display(&bagl_ui_idle[currentUiElement++]);
                break;
            }
        } else if (uiState == UI_ADDRESS) {
            if (currentUiElement <
                (sizeof(bagl_ui_address) / sizeof(bagl_element_t))) {
                io_seproxyhal_display(&bagl_ui_address[currentUiElement++]);
                break;
            }
        } else if (uiState == UI_APPROVAL) {
            if (currentUiElement <
                (sizeof(bagl_ui_approval) / sizeof(bagl_element_t))) {
                io_seproxyhal_display(&bagl_ui_approval[currentUiElement++]);
                break;
            }
        } else {
            screen_printf("Unknown UI state\n");
        }
        if (usb_enable_request) {
            io_usb_enable(1);
            usb_enable_request = 0;
        }
        goto general_status;

    // unknown events are acknowledged
    default:
    general_status:
        // send a general status last command
        offset = 0;
        G_io_seproxyhal_spi_buffer[offset++] = SEPROXYHAL_TAG_GENERAL_STATUS;
        G_io_seproxyhal_spi_buffer[offset++] = 0;
        G_io_seproxyhal_spi_buffer[offset++] = 2;
        G_io_seproxyhal_spi_buffer[offset++] =
            SEPROXYHAL_TAG_GENERAL_STATUS_LAST_COMMAND >> 8;
        G_io_seproxyhal_spi_buffer[offset++] =
            SEPROXYHAL_TAG_GENERAL_STATUS_LAST_COMMAND;
        io_seproxyhal_spi_send(G_io_seproxyhal_spi_buffer, offset);
        element_displayed = 0;
        break;
    }
    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    usb_enable_request = 0;
    element_displayed = 0;
    uiState = UI_IDLE;
    currentUiElement = 0;
    os_memset(&txContext, 0, sizeof(txContext));

    // ensure exception will work as planned
    os_boot();

    BEGIN_TRY {
        TRY {
            io_seproxyhal_init();

#ifdef HAVE_BLE

            // send BLE power on (default parameters)
            G_io_seproxyhal_spi_buffer[0] = SEPROXYHAL_TAG_BLE_RADIO_POWER;
            G_io_seproxyhal_spi_buffer[1] = 0;
            G_io_seproxyhal_spi_buffer[2] = 1;
            G_io_seproxyhal_spi_buffer[3] = 3; // ble on & advertise
            io_seproxyhal_spi_send(G_io_seproxyhal_spi_buffer, 4);
#endif

            usb_enable_request = 1;
            io_seproxyhal_display(&bagl_ui_erase_all[0]);

            sample_main();
        }
        CATCH_OTHER(e) {
        }
        FINALLY {
        }
    }
    END_TRY;
}
