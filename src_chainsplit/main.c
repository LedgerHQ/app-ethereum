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
#include "ethUstream.h"
#include "ethUtils.h"
#include "uint256.h"

#include "os_io_seproxyhal.h"
#include "string.h"
unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

unsigned int io_seproxyhal_touch_exit(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_tx_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_tx_cancel(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_address_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_address_cancel(const bagl_element_t *e);

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

#define WEI_TO_ETHER 18

typedef struct publicKeyContext_t {
    cx_ecfp_public_key_t publicKey;
    uint8_t address[41];
} publicKeyContext_t;

typedef struct transactionContext_t {
    uint8_t pathLength;
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t hash[32];
} transactionContext_t;

typedef struct splitContext_t {
    uint8_t data[4 + 32 + 32];
    uint8_t targetAddress[20];
    bool targetAddressProvided;
} splitContext_t;

union {
    publicKeyContext_t publicKeyContext;
    transactionContext_t transactionContext;
} tmpCtx;
txContext_t txContext;
txContent_t txContent;
splitContext_t splitContext;
cx_sha3_t sha3;
volatile char addressSummary[21];
volatile char address1[21];
volatile char address2[21];
volatile char fullAmount[50];
volatile char splitType[50];
volatile char gasPrice[50];
volatile char startgas[50];

ux_state_t ux;
// display stepped screens
unsigned int ux_step;
unsigned int ux_step_count;

static const char *const SPLIT_TO_ETH = "Split to ETH";
static const char *const SPLIT_TO_ETC = "Split to ETC";

static const uint8_t const SPLIT_TRANSFER_ID[] = {0x9c, 0x70, 0x93, 0x43};
static const uint8_t const SPLIT_ADDRESS[] = {
    0x5d, 0xc8, 0x10, 0x8f, 0xc7, 0x90, 0x18, 0x11, 0x3a, 0x58,
    0x32, 0x8f, 0x52, 0x83, 0xb3, 0x76, 0xb8, 0x39, 0x22, 0xef};

// UI displayed when no signature proposal has been received
static const bagl_element_t const ui_idle_blue[] = {
    {{BAGL_RECTANGLE, 0x00, 0, 0, 320, 480, 0, 0, BAGL_FILL, 0xf9f9f9, 0xf9f9f9,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

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
     "ETC<>ETH split",
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
     io_seproxyhal_touch_exit,
     NULL,
     NULL}};

unsigned int ui_idle_blue_button(unsigned int button_mask,
                                 unsigned int button_mask_counter) {
    return 0;
}

const bagl_element_t ui_idle_nanos[] = {
    // type                               userid    x    y   w    h  str rad
    // fill      fg        bg      fid iid  txt   touchparams...       ]
    {{BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x01, 17, 9, 14, 14, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_ETHEREUM_BADGE},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 38, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px, 0},
     "Use wallet to",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 39, 26, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px, 0},
     "split ETH<>ETC",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_ICON, 0x01, 118, 14, 7, 4, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_DOWN},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x02, 29, 9, 14, 14, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_DASHBOARD_BADGE},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    //{{BAGL_LABELINE                       , 0x02,   0,   3, 128,  32, 0, 0, 0
    //, 0xFFFFFF, 0x000000,
    //BAGL_FONT_OPEN_SANS_REGULAR_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, "view
    //accounts", 0, 0, 0, NULL, NULL, NULL },
    {{BAGL_LABELINE, 0x02, 50, 19, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px, 0},
     "Quit app",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_ICON, 0x02, 3, 14, 7, 4, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_UP},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
};
unsigned int ui_idle_nanos_button(unsigned int button_mask,
                                  unsigned int button_mask_counter);

unsigned int ui_idle_nanos_state;
unsigned int ui_idle_nanos_prepro(const bagl_element_t *element) {
    if (element->component.userid > 0) {
        return (ui_idle_nanos_state == element->component.userid - 1);
    }
    return 1;
}

bagl_element_t const ui_address_blue[] = {
    {{BAGL_RECTANGLE, 0x00, 0, 0, 320, 480, 0, 0, BAGL_FILL, 0xf9f9f9, 0xf9f9f9,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

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
     "ETC<>ETH split",
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
     io_seproxyhal_touch_address_cancel,
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
     io_seproxyhal_touch_address_ok,
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
unsigned int ui_address_blue_button(unsigned int button_mask,
                                    unsigned int button_mask_counter) {
    return 0;
}

const bagl_element_t ui_address_nanos[] = {
    // type                               userid    x    y   w    h  str rad
    // fill      fg        bg      fid iid  txt   touchparams...       ]
    {{BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x01, 31, 9, 14, 14, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_EYE_BADGE},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 52, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px, 0},
     "Confirm",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 53, 26, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px, 0},
     "account",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x02, 0, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Account",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x02, 0, 26, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     addressSummary,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CROSS},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CHECK},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
};

unsigned int ui_address_prepro(const bagl_element_t *element) {
    if (element->component.userid > 0) {
        unsigned int display = (ux_step == element->component.userid - 1);
        if (display) {
            switch (element->component.userid) {
            case 1:
                io_seproxyhal_setup_ticker(2000);
                break;
            case 2:
                io_seproxyhal_setup_ticker(3000);
                break;
            }
        }
        return display;
    }
    return 1;
}

unsigned int ui_address_nanos_button(unsigned int button_mask,
                                     unsigned int button_mask_counter);

// UI to approve or deny the signature proposal
static const bagl_element_t const ui_approval_blue[] = {
    {{BAGL_RECTANGLE, 0x00, 0, 0, 320, 480, 0, 0, BAGL_FILL, 0xf9f9f9, 0xf9f9f9,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

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
     "ETC<>ETH split",
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
     io_seproxyhal_touch_tx_cancel,
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
     io_seproxyhal_touch_tx_ok,
     NULL,
     NULL},

    {{BAGL_LABEL, 0x00, 0, 87, 320, 32, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "CONFIRM SPLIT",
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
    {{BAGL_LABEL, 0x00, 0, 265, 320, 32, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_14px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Gas price / start",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABEL, 0x00, 0, 305, 320, 33, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_16px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     (const char *)gasPrice,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABEL, 0x00, 0, 325, 320, 33, 0, 0, 0, 0x000000, 0xF9F9F9,
      BAGL_FONT_OPEN_SANS_LIGHT_16px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     (const char *)startgas,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

};
unsigned int ui_approval_blue_button(unsigned int button_mask,
                                     unsigned int button_mask_counter) {
    return 0;
}

const bagl_element_t ui_approval_nanos[] = {
    // type                               userid    x    y   w    h  str rad
    // fill      fg        bg      fid iid  txt   touchparams...       ]
    {{BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CROSS},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CHECK},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x01, 21, 9, 14, 14, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_TRANSACTION_BADGE},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 42, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px, 0},
     "Confirm",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 43, 26, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px, 0},
     splitType,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x02, 0, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Amount",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x02, 23, 26, 82, 11, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     fullAmount,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x03, 0, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Recipient account",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x03, 16, 26, 96, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     addressSummary,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x04, 0, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Gas price",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x04, 23, 26, 82, 11, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     gasPrice,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x05, 0, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Gas limit",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x05, 23, 26, 82, 11, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     startgas,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
};

unsigned int ui_approval_prepro(const bagl_element_t *element) {
    if (element->component.userid > 0) {
        unsigned int display = (ux_step == element->component.userid - 1);
        if (display) {
            switch (element->component.userid) {
            case 1:
                io_seproxyhal_setup_ticker(2000);
                break;
            case 3:
                io_seproxyhal_setup_ticker(3000);
                break;
            case 2:
                io_seproxyhal_setup_ticker(MAX(
                    3000, 1000 + bagl_label_roundtrip_duration_ms(element, 7)));
                break;
            case 4:
                io_seproxyhal_setup_ticker(MAX(
                    3000, 1000 + bagl_label_roundtrip_duration_ms(element, 7)));
                break;
            case 5:
                io_seproxyhal_setup_ticker(MAX(
                    3000, 1000 + bagl_label_roundtrip_duration_ms(element, 7)));
                break;
            }
        }
        return display;
    }
    return 1;
}

unsigned int ui_approval_nanos_button(unsigned int button_mask,
                                      unsigned int button_mask_counter);

void ui_idle(void) {
    if (os_seph_features() &
        SEPROXYHAL_TAG_SESSION_START_EVENT_FEATURE_SCREEN_BIG) {
        UX_DISPLAY(ui_idle_blue, NULL);
    } else {
        ui_idle_nanos_state = 0; // start by displaying the idle first screen
        UX_DISPLAY(ui_idle_nanos, ui_idle_nanos_prepro);
    }
}

unsigned int io_seproxyhal_touch_exit(const bagl_element_t *e) {
    // Go back to the dashboard
    os_sched_exit(0);
    return 0; // do not redraw the widget
}

unsigned int ui_idle_nanos_button(unsigned int button_mask,
                                  unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT: // UP
        if (ui_idle_nanos_state == 1) {
            ui_idle_nanos_state = 0;
            UX_DISPLAY(ui_idle_nanos, ui_idle_nanos_prepro);
        }
        break;

    case BUTTON_EVT_RELEASED | BUTTON_RIGHT: // DOWN
        if (ui_idle_nanos_state == 0) {
            ui_idle_nanos_state = 1;
            UX_DISPLAY(ui_idle_nanos, ui_idle_nanos_prepro);
        }
        break;

    case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT: // EXIT
        if (ui_idle_nanos_state == 1) {
            io_seproxyhal_touch_exit(NULL);
        }
        break;
    }
    return 0;
}

unsigned int io_seproxyhal_touch_address_ok(const bagl_element_t *e) {
    uint32_t tx = set_result_get_publicKey();
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_address_cancel(const bagl_element_t *e) {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

unsigned int ui_address_nanos_button(unsigned int button_mask,
                                     unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT: // CANCEL
        io_seproxyhal_touch_address_cancel(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_RIGHT: { // OK
        io_seproxyhal_touch_address_ok(NULL);
        break;
    }
    }
    return 0;
}

unsigned int io_seproxyhal_touch_tx_ok(const bagl_element_t *e) {
    uint8_t privateKeyData[32];
    uint8_t signature[100];
    uint8_t signatureLength;
    cx_ecfp_private_key_t privateKey;
    uint32_t tx = 0;
    uint8_t rLength, sLength, rOffset, sOffset;
    os_perso_derive_node_bip32(
        CX_CURVE_256K1, tmpCtx.transactionContext.bip32Path,
        tmpCtx.transactionContext.pathLength, privateKeyData, NULL);
    cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
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
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_tx_cancel(const bagl_element_t *e) {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

unsigned int ui_approval_nanos_button(unsigned int button_mask,
                                      unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        io_seproxyhal_touch_tx_cancel(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_RIGHT: {
        io_seproxyhal_touch_tx_ok(NULL);
        break;
    }
    }
    return 0;
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
    if (context->currentFieldLength != sizeof(splitContext.data)) {
        screen_printf("Invalid length for RLP_DATA\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, splitContext.data + context->currentFieldPos,
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
                flags = 0;

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
                    os_perso_derive_node_bip32(CX_CURVE_256K1, bip32Path,
                                               bip32PathLength, privateKeyData,
                                               NULL);
                    cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32,
                                             &privateKey);
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
                        addressSummary[0] = '0';
                        addressSummary[1] = 'x';
                        os_memmove((unsigned char *)(addressSummary + 2),
                                   tmpCtx.publicKeyContext.address, 4);
                        os_memmove((unsigned char *)(addressSummary + 6), "...",
                                   3);
                        os_memmove((unsigned char *)(addressSummary + 9),
                                   tmpCtx.publicKeyContext.address + 40 - 4, 4);
                        addressSummary[13] = '\0';
                        os_memmove((unsigned char *)address1,
                                   tmpCtx.publicKeyContext.address, 20);
                        address1[20] = '\0';
                        os_memmove((unsigned char *)address2,
                                   tmpCtx.publicKeyContext.address + 20, 20);
                        address2[20] = '\0';

                        // prepare for a UI based reply
                        if (os_seph_features() &
                            SEPROXYHAL_TAG_SESSION_START_EVENT_FEATURE_SCREEN_BIG) {
                            UX_DISPLAY(ui_address_blue, NULL);
                        } else {
                            ux_step = 0;
                            ux_step_count = 2;
                            // io_seproxyhal_setup_ticker(2000);
                            UX_DISPLAY(ui_address_nanos, ui_address_prepro);
                        }

                        flags |= IO_ASYNCH_REPLY;
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
                    os_perso_derive_node_bip32(CX_CURVE_256K1, bip32Path,
                                               bip32PathLength, privateKeyData,
                                               NULL);
                    cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32,
                                             &privateKey);
                    cx_ecfp_generate_pair(CX_CURVE_256K1,
                                          &tmpCtx.publicKeyContext.publicKey,
                                          &privateKey, 1);
                    os_memset(&privateKey, 0, sizeof(privateKey));
                    os_memset(privateKeyData, 0, sizeof(privateKeyData));
                    getEthAddressFromKey(&tmpCtx.publicKeyContext.publicKey,
                                         splitContext.targetAddress, &sha3);
                    splitContext.targetAddressProvided = true;
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
                    cx_hash((cx_hash_t *)&sha3, CX_LAST,
                            tmpCtx.transactionContext.hash, 0,
                            tmpCtx.transactionContext.hash);
                    // Abort if not sending to the splitting contract
                    if (os_memcmp(txContent.destination, SPLIT_ADDRESS,
                                  sizeof(SPLIT_ADDRESS)) != 0) {
                        screen_printf("Invalid target address\n");
                        THROW(0x6A80);
                    }
                    // Abort if not calling the split function
                    if (os_memcmp(splitContext.data, SPLIT_TRANSFER_ID,
                                  sizeof(SPLIT_TRANSFER_ID)) != 0) {
                        screen_printf("Invalid contract call ID\n");
                        THROW(0x6A80);
                    }
                    // Abort if the destination address was not provided
                    if (!splitContext.targetAddressProvided ||
                        (os_memcmp(splitContext.targetAddress,
                                   splitContext.data + 4 + 32 + 12, 20) != 0)) {
                        screen_printf("Invalid target address\n");
                        THROW(0x6A80);
                    }
                    // Check where the split is made
                    switch (splitContext.data[4 + 31]) {
                    case 0x00:
                        strcpy(splitType, SPLIT_TO_ETC);
                        break;
                    case 0x01:
                        strcpy(splitType, SPLIT_TO_ETH);
                        break;
                    default:
                        screen_printf("Invalid boolean\n");
                        THROW(0x6A80);
                    }
                    // Add target address
                    getEthAddressStringFromBinary(splitContext.targetAddress,
                                                  address, &sha3);
                    addressSummary[0] = '0';
                    addressSummary[1] = 'x';
                    os_memmove((unsigned char *)(addressSummary + 2), address,
                               4);
                    os_memmove((unsigned char *)(addressSummary + 6), "...", 3);
                    os_memmove((unsigned char *)(addressSummary + 9),
                               address + 40 - 4, 4);
                    addressSummary[13] = '\0';
                    os_memmove((unsigned char *)address1, address, 20);
                    address1[20] = '\0';
                    os_memmove((unsigned char *)address2, address + 20, 20);
                    address2[20] = '\0';
                    // Add amount in ethers
                    convertUint256BE(txContent.value.value,
                                     txContent.value.length, &uint256);
                    tostring256(&uint256, 10, (char *)(G_io_apdu_buffer + 100),
                                100);
                    i = 0;
                    while (G_io_apdu_buffer[100 + i]) {
                        i++;
                    }
                    adjustDecimals((char *)(G_io_apdu_buffer + 100), i,
                                   (char *)G_io_apdu_buffer, 100, WEI_TO_ETHER);
                    i = 0;
                    fullAmount[0] = 'E';
                    fullAmount[1] = 'T';
                    fullAmount[2] = 'H';
                    fullAmount[3] = ' ';
                    while (G_io_apdu_buffer[i]) {
                        fullAmount[4 + i] = G_io_apdu_buffer[i];
                        i++;
                    }
                    fullAmount[4 + i] = '\0';
                    // Add gas price in ethers
                    convertUint256BE(txContent.gasprice.value,
                                     txContent.gasprice.length, &uint256);
                    tostring256(&uint256, 10, (char *)(G_io_apdu_buffer + 100),
                                100);
                    i = 0;
                    while (G_io_apdu_buffer[100 + i]) {
                        i++;
                    }
                    adjustDecimals((char *)(G_io_apdu_buffer + 100), i,
                                   (char *)G_io_apdu_buffer, 100, WEI_TO_ETHER);
                    i = 0;
                    gasPrice[0] = 'E';
                    gasPrice[1] = 'T';
                    gasPrice[2] = 'H';
                    gasPrice[3] = ' ';
                    while (G_io_apdu_buffer[i]) {
                        gasPrice[4 + i] = G_io_apdu_buffer[i];
                        i++;
                    }
                    gasPrice[4 + i] = '\0';
                    // Add gas limit in native format
                    convertUint256BE(txContent.startgas.value,
                                     txContent.startgas.length, &uint256);
                    tostring256(&uint256, 10, (char *)G_io_apdu_buffer, 100);
                    i = 0;
                    while (G_io_apdu_buffer[i]) {
                        startgas[i] = G_io_apdu_buffer[i];
                        i++;
                    }
                    startgas[i] = '\0';

                    if (os_seph_features() &
                        SEPROXYHAL_TAG_SESSION_START_EVENT_FEATURE_SCREEN_BIG) {
                        UX_DISPLAY(ui_approval_blue, NULL);
                    } else {
                        ux_step = 0;
                        ux_step_count = 5;
                        //  	                    io_seproxyhal_setup_ticker(2000);
                        UX_DISPLAY(ui_approval_nanos, ui_approval_prepro);
                    }

                    flags |= IO_ASYNCH_REPLY;
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
    return io_seproxyhal_display_default((bagl_element_t *)element);
}

unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_FINGER_EVENT:
        UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        if (UX_DISPLAYED()) {
            // TODO perform actions after all screen elements have been
            // displayed
        } else {
            UX_DISPLAY_PROCESSED_EVENT();
        }
        break;

    case SEPROXYHAL_TAG_TICKER_EVENT:
        // prepare next screen
        ux_step = (ux_step + 1) % ux_step_count;
        // redisplay screen
        UX_REDISPLAY();
        break;

    // unknown events are acknowledged
    default:
        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

void app_exit(void) {
    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {
        }
    }
    END_TRY_L(exit);
}

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    os_memset(&txContext, 0, sizeof(txContext));
    os_memset(&splitContext, 0, sizeof(splitContext));

    UX_INIT();

    // ensure exception will work as planned
    os_boot();

    BEGIN_TRY {
        TRY {
            io_seproxyhal_init();

            USB_power(1);

            ui_idle();

            sample_main();
        }
        CATCH_OTHER(e) {
        }
        FINALLY {
        }
    }
    END_TRY;

    app_exit();

    return 0;
}
