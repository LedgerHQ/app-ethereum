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
#include "stdbool.h"
#include "ethUstream.h"
#include "ethUtils.h"
#include "uint256.h"
#include "tokens.h"
#include "chainConfig.h"

#include "os_io_seproxyhal.h"
#include "string.h"

#include "glyphs.h"

#define __NAME3(a, b, c) a##b##c
#define NAME3(a, b, c) __NAME3(a, b, c)

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];


unsigned int io_seproxyhal_touch_settings(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_exit(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_tx_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_tx_cancel(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_address_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_address_cancel(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_signMessage_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_signMessage_cancel(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_data_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_data_cancel(const bagl_element_t *e);
void ui_idle(void);

uint32_t set_result_get_publicKey(void);
void finalizeParsing(bool);

#define MAX_BIP32_PATH 10

#define CLA 0xE0
#define INS_GET_PUBLIC_KEY 0x02
#define INS_SIGN 0x04
#define INS_GET_APP_CONFIGURATION 0x06
#define INS_SIGN_PERSONAL_MESSAGE 0x08
#define P1_CONFIRM 0x01
#define P1_NON_CONFIRM 0x00
#define P2_NO_CHAINCODE 0x00
#define P2_CHAINCODE 0x01
#define P1_FIRST 0x00
#define P1_MORE 0x80

#define OFFSET_CLA 0
#define OFFSET_INS 1
#define OFFSET_P1 2
#define OFFSET_P2 3
#define OFFSET_LC 4
#define OFFSET_CDATA 5

#define WEI_TO_ETHER 18

static const uint8_t const TOKEN_TRANSFER_ID[] = { 0xa9, 0x05, 0x9c, 0xbb };
typedef struct tokenContext_t {
    uint8_t data[4 + 32 + 32];
    uint32_t dataFieldPos;    
} tokenContext_t;

typedef struct rawDataContext_t { 
    uint8_t data[32];
    uint8_t fieldIndex;
    uint8_t fieldOffset;
} rawDataContext_t;

typedef struct publicKeyContext_t {
    cx_ecfp_public_key_t publicKey;
    uint8_t address[41];
    uint8_t chainCode[32];
    bool getChaincode;
} publicKeyContext_t;

typedef struct transactionContext_t {
    uint8_t pathLength;
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t hash[32];
} transactionContext_t;

typedef struct messageSigningContext_t {
    uint8_t pathLength;
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t hash[32];
    uint32_t remainingLength;    
} messageSigningContext_t;

union {
    publicKeyContext_t publicKeyContext;
    transactionContext_t transactionContext;
    messageSigningContext_t messageSigningContext;
} tmpCtx;
txContext_t txContext;

union {
  txContent_t txContent;
  cx_sha256_t sha2;
} tmpContent;

cx_sha3_t sha3;

union {
    tokenContext_t tokenContext;
    rawDataContext_t rawDataContext;    
} dataContext;

volatile uint8_t dataAllowed;
volatile uint8_t contractDetails;
volatile char addressSummary[32];
volatile bool dataPresent;
volatile bool tokenProvisioned;

bagl_element_t tmp_element;

ux_state_t ux;
// display stepped screens
unsigned int ux_step;
unsigned int ux_step_count;

typedef struct internalStorage_t {
  unsigned char dataAllowed;
  unsigned char contractDetails;
  uint8_t initialized;
} internalStorage_t;

typedef struct strData_t {
    char fullAddress[43];
    char fullAmount[50];
    char maxFee[50];
} strData_t;

typedef struct strDataTmp_t {
    char tmp[100];
    char tmp2[40];
} strDataTmp_t;

union {
    strData_t common;
    strDataTmp_t tmp;
} strings;

WIDE internalStorage_t N_storage_real;
#define N_storage (*(WIDE internalStorage_t*) PIC(&N_storage_real)) 

static const char const CONTRACT_ADDRESS[] = "New contract";

static const char const SIGN_MAGIC[] = "\x19"
                                       "Ethereum Signed Message:\n";

const unsigned char hex_digits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

chain_config_t *chainConfig;                                    

void array_hexstr(char *strbuf, const void *bin, unsigned int len) {
    while (len--) {
        *strbuf++ = hex_digits[((*((char *)bin)) >> 4) & 0xF];
        *strbuf++ = hex_digits[(*((char *)bin)) & 0xF];
        bin = (const void *)((unsigned int)bin + 1);
    }
    *strbuf = 0; // EOS
}


const bagl_element_t* ui_menu_item_out_over(const bagl_element_t* e) {
  // the selection rectangle is after the none|touchable
  e = (const bagl_element_t*)(((unsigned int)e)+sizeof(bagl_element_t));
  return e;
}


#define BAGL_FONT_OPEN_SANS_LIGHT_16_22PX_AVG_WIDTH 10
#define BAGL_FONT_OPEN_SANS_REGULAR_10_13PX_AVG_WIDTH 8
#define MAX_CHAR_PER_LINE 25

#define COLOR_BG_1 0xF9F9F9
#define COLOR_APP 0x0ebdcf
#define COLOR_APP_LIGHT 0x87dee6

#if defined(TARGET_BLUE)

unsigned int map_color(unsigned int color) {
    switch(color) {
        case COLOR_APP:
            return chainConfig->color_header;

        case COLOR_APP_LIGHT:
            return chainConfig->color_dashboard;
    }
    return color;
}
void copy_element_and_map_coin_colors(const bagl_element_t* element) {
    os_memmove(&tmp_element, element, sizeof(bagl_element_t));
    tmp_element.component.fgcolor = map_color(tmp_element.component.fgcolor);
    tmp_element.component.bgcolor = map_color(tmp_element.component.bgcolor);
    tmp_element.overfgcolor = map_color(tmp_element.overfgcolor);
    tmp_element.overbgcolor = map_color(tmp_element.overbgcolor);
}

const bagl_element_t *ui_idle_blue_prepro(const bagl_element_t *element) {
    copy_element_and_map_coin_colors(element);
    if (element->component.userid == 0x01) {
        tmp_element.text = chainConfig->header_text;
    }
    return &tmp_element;
}

const bagl_element_t ui_idle_blue[] = {
  // type                               userid    x    y   w    h  str rad fill      fg        bg      fid iid  txt   touchparams...       ]
  {{BAGL_RECTANGLE                      , 0x00,   0,  68, 320, 413, 0, 0, BAGL_FILL, COLOR_BG_1, 0x000000, 0                                                                                 , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL },

  // erase screen (only under the status bar)
  {{BAGL_RECTANGLE                      , 0x00,   0,  20, 320,  48, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP, 0                                                      , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL},

  /// TOP STATUS BAR
  {{BAGL_LABELINE                       , 0x01,   0,  45, 320,  30, 0, 0, BAGL_FILL, 0xFFFFFF, COLOR_APP, BAGL_FONT_OPEN_SANS_SEMIBOLD_10_13PX|BAGL_FONT_ALIGNMENT_CENTER, 0   }, CHAINID_UPCASE, 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00,   0,  19,  56,  44, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP_LIGHT, BAGL_FONT_SYMBOLS_0|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, BAGL_FONT_SYMBOLS_0_SETTINGS, 0, COLOR_APP, 0xFFFFFF, io_seproxyhal_touch_settings, NULL, NULL},
  {{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00, 264,  19,  56,  44, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP_LIGHT, BAGL_FONT_SYMBOLS_0|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, BAGL_FONT_SYMBOLS_0_DASHBOARD, 0, COLOR_APP, 0xFFFFFF, io_seproxyhal_touch_exit, NULL, NULL},

  // BADGE_<CHAINID>.GIF
  //{{BAGL_ICON                           , 0x00, 135, 178,  50,  50, 0, 0, BAGL_FILL, 0       , COLOR_BG_1, 0                                                              ,0   } , &NAME3(C_blue_badge_, CHAINID, ), 0, 0, 0, NULL, NULL, NULL },

  {{BAGL_LABELINE                       , 0x00,   0, 270, 320,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_LIGHT_16_22PX|BAGL_FONT_ALIGNMENT_CENTER, 0   }, "Open your wallet", 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x00,   0, 308, 320,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX|BAGL_FONT_ALIGNMENT_CENTER, 0   }, "Connect your Ledger Blue and open your", 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x00,   0, 331, 320,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX|BAGL_FONT_ALIGNMENT_CENTER, 0   }, "preferred wallet to view your accounts.", 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_LABELINE                       , 0x00,   0, 450, 320,  14, 0, 0, 0        , 0x999999, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_8_11PX|BAGL_FONT_ALIGNMENT_CENTER, 0   }, "Validation requests will show automatically.", 10, 0, COLOR_BG_1, NULL, NULL, NULL },
};

unsigned int ui_idle_blue_button(unsigned int button_mask, unsigned int button_mask_counter) {
  return 0;
}
#endif // #if defined(TARGET_BLUE)


#if defined(TARGET_NANOS)


const ux_menu_entry_t menu_main[];
const ux_menu_entry_t menu_settings[];
//const ux_menu_entry_t menu_settings_browser[];
const ux_menu_entry_t menu_settings_data[];
const ux_menu_entry_t menu_settings_details[];

#ifdef HAVE_U2F

// change the setting
void menu_settings_data_change(unsigned int enabled) {
  dataAllowed = enabled;
  nvm_write(&N_storage.dataAllowed, (void*)&dataAllowed, sizeof(uint8_t));
  // go back to the menu entry
  UX_MENU_DISPLAY(0, menu_settings, NULL);
}

void menu_settings_details_change(unsigned int enabled) {
  contractDetails = enabled;
  nvm_write(&N_storage.contractDetails, (void*)&contractDetails, sizeof(uint8_t));
  // go back to the menu entry
  UX_MENU_DISPLAY(0, menu_settings, NULL);
}

// show the currently activated entry
void menu_settings_data_init(unsigned int ignored) {
  UNUSED(ignored);
  UX_MENU_DISPLAY(N_storage.dataAllowed?1:0, menu_settings_data, NULL);
}

void menu_settings_details_init(unsigned int ignored) {
  UNUSED(ignored);
  UX_MENU_DISPLAY(N_storage.contractDetails?1:0, menu_settings_details, NULL);
}

const ux_menu_entry_t menu_settings_data[] = {
  {NULL, menu_settings_data_change, 0, NULL, "No", NULL, 0, 0},
  {NULL, menu_settings_data_change, 1, NULL, "Yes", NULL, 0, 0},
  UX_MENU_END
};

const ux_menu_entry_t menu_settings_details[] = {
  {NULL, menu_settings_details_change, 0, NULL, "No", NULL, 0, 0},
  {NULL, menu_settings_details_change, 1, NULL, "Yes", NULL, 0, 0},
  UX_MENU_END
};

const ux_menu_entry_t menu_settings[] = {
  {NULL, menu_settings_data_init, 0, NULL, "Contract data", NULL, 0, 0},
  {NULL, menu_settings_details_init, 0, NULL, "Display data", NULL, 0, 0},
  {menu_main, NULL, 1, &C_icon_back, "Back", NULL, 61, 40},
  UX_MENU_END
};
#endif // HAVE_U2F

const ux_menu_entry_t menu_about[] = {
  {NULL, NULL, 0, NULL, "Version", APPVERSION , 0, 0},
  {menu_main, NULL, 2, &C_icon_back, "Back", NULL, 61, 40},
  UX_MENU_END
};

const ux_menu_entry_t menu_main[] = {
  //{NULL, NULL, 0, &NAME3(C_nanos_badge_, CHAINID, ), "Use wallet to", "view accounts", 33, 12},
  {NULL, NULL, 0, NULL, "Use wallet to", "view accounts", 0, 0},
  {menu_settings, NULL, 0, NULL, "Settings", NULL, 0, 0},
  {menu_about, NULL, 0, NULL, "About", NULL, 0, 0},
  {NULL, os_sched_exit, 0, &C_icon_dashboard, "Quit app", NULL, 50, 29},
  UX_MENU_END
};

#endif // #if defined(TARGET_NANOS)

#if defined(TARGET_BLUE)
const bagl_element_t * ui_settings_blue_toggle_data(const bagl_element_t * e) {
  // swap setting and request redraw of settings elements
  uint8_t setting = N_storage.dataAllowed?0:1;
  nvm_write(&N_storage.dataAllowed, (void*)&setting, sizeof(uint8_t));

  // only refresh settings mutable drawn elements
  UX_REDISPLAY_IDX(7);

  // won't redisplay the bagl_none
  return 0;
}

const bagl_element_t * ui_settings_blue_toggle_details(const bagl_element_t * e) {
  // swap setting and request redraw of settings elements
  uint8_t setting = N_storage.contractDetails?0:1;
  nvm_write(&N_storage.contractDetails, (void*)&setting, sizeof(uint8_t));

  // only refresh settings mutable drawn elements
  UX_REDISPLAY_IDX(7);

  // won't redisplay the bagl_none
  return 0;
}


// don't perform any draw/color change upon finger event over settings
const bagl_element_t* ui_settings_out_over(const bagl_element_t* e) {
  return NULL;
}

unsigned int ui_settings_back_callback(const bagl_element_t* e) {
  // go back to idle
  ui_idle();
  return 0;
}

const bagl_element_t ui_settings_blue[] = {
  // type                               userid    x    y   w    h  str rad fill      fg        bg      fid iid  txt   touchparams...       ]
  {{BAGL_RECTANGLE                      , 0x00,   0,  68, 320, 413, 0, 0, BAGL_FILL, COLOR_BG_1, 0x000000, 0                                                                                 , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL },

  // erase screen (only under the status bar)
  {{BAGL_RECTANGLE                      , 0x00,   0,  20, 320,  48, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP, 0                                                      , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL},

  /// TOP STATUS BAR
  {{BAGL_LABELINE                       , 0x00,   0,  45, 320,  30, 0, 0, BAGL_FILL, 0xFFFFFF, COLOR_APP, BAGL_FONT_OPEN_SANS_SEMIBOLD_10_13PX|BAGL_FONT_ALIGNMENT_CENTER, 0   }, "SETTINGS", 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00,   0,  19,  50,  44, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP_LIGHT, BAGL_FONT_SYMBOLS_0|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, BAGL_FONT_SYMBOLS_0_LEFT, 0, COLOR_APP, 0xFFFFFF, ui_settings_back_callback, NULL, NULL},
  //{{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00, 264,  19,  56,  44, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP_LIGHT, BAGL_FONT_SYMBOLS_0|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, BAGL_FONT_SYMBOLS_0_DASHBOARD, 0, COLOR_APP, 0xFFFFFF, io_seproxyhal_touch_exit, NULL, NULL},


  {{BAGL_LABELINE                       , 0x00,  30, 105, 160,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, "Contract data", 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x00,  30, 126, 260,  30, 0, 0, BAGL_FILL, 0x999999, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_8_11PX, 0   }, "Allow contract data in transactions", 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_NONE   | BAGL_FLAG_TOUCHABLE   , 0x00,   0,  78, 320,  68, 0, 0, BAGL_FILL, 0xFFFFFF, 0x000000, 0                                                                                        , 0   }, NULL, 0, 0xEEEEEE, 0x000000, ui_settings_blue_toggle_data, ui_settings_out_over, ui_settings_out_over },

  {{BAGL_RECTANGLE, 0x00, 30, 146, 260, 1, 1, 0, 0, 0xEEEEEE, COLOR_BG_1, 0, 0}, NULL, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE, 0x00, 30, 174, 160, 30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0}, "Display data", 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE, 0x00, 30, 195, 260, 30, 0, 0, BAGL_FILL, 0x999999, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_8_11PX, 0}, "Display contract data details", 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_NONE | BAGL_FLAG_TOUCHABLE, 0x00, 0, 147, 320, 68, 0, 0, BAGL_FILL, 0xFFFFFF, 0x000000, 0, 0}, NULL, 0, 0xEEEEEE, 0x000000, ui_settings_blue_toggle_details, ui_settings_out_over, ui_settings_out_over},

  {{BAGL_ICON, 0x02, 258, 167, 32, 18, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, 0, 0}, NULL, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_ICON                           , 0x01, 258,  98,  32,  18, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, 0, 0   }, NULL, 0, 0, 0, NULL, NULL, NULL},
};

const bagl_element_t * ui_settings_blue_prepro(const bagl_element_t * e) {
  copy_element_and_map_coin_colors(e);
  // none elements are skipped
  if ((e->component.type&(~BAGL_FLAG_TOUCHABLE)) == BAGL_NONE) {
    return 0;
  }
  // swap icon buffer to be displayed depending on if corresponding setting is enabled or not.
  if (e->component.userid) {
    switch(e->component.userid) {
      case 0x01:
        // swap icon content
        if (N_storage.dataAllowed) {
          tmp_element.text = &C_icon_toggle_set;
        }
        else {
          tmp_element.text = &C_icon_toggle_reset;
        }
        break;
      case 0x02:
        // swap icon content
        if (N_storage.contractDetails) {
          tmp_element.text = &C_icon_toggle_set;
        }
        else {
          tmp_element.text = &C_icon_toggle_reset;
        }
        break;
    }
  }
  return &tmp_element;
}

unsigned int ui_settings_blue_button(unsigned int button_mask, unsigned int button_mask_counter) {
  return 0;  
}
#endif // #if defined(TARGET_BLUE)




#if defined(TARGET_BLUE)
// reuse addressSummary for each line content
const char* ui_details_title;
const char* ui_details_content;
typedef void (*callback_t)(void);
callback_t ui_details_back_callback;

const bagl_element_t* ui_details_blue_back_callback(const bagl_element_t* element) {
  ui_details_back_callback();
  return 0;
}


const bagl_element_t ui_details_blue[] = {
  // erase screen (only under the status bar)
  {{BAGL_RECTANGLE                      , 0x00,   0,  68, 320, 413, 0, 0, BAGL_FILL, COLOR_BG_1, 0x000000, 0                                                                                 , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL },

  {{BAGL_RECTANGLE                      , 0x00,   0,  20, 320,  48, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP, 0                                                      , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL},

  /// TOP STATUS BAR
  {{BAGL_LABELINE                       , 0x01,   0,  45, 320,  30, 0, 0, BAGL_FILL, 0xFFFFFF, COLOR_APP, BAGL_FONT_OPEN_SANS_SEMIBOLD_10_13PX|BAGL_FONT_ALIGNMENT_CENTER, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00,   0,  19,  50,  44, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP_LIGHT, BAGL_FONT_SYMBOLS_0|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, BAGL_FONT_SYMBOLS_0_LEFT, 0, COLOR_APP, 0xFFFFFF, ui_details_blue_back_callback, NULL, NULL},
  //{{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00, 264,  19,  56,  44, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP_LIGHT, BAGL_FONT_SYMBOLS_0|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, " " /*BAGL_FONT_SYMBOLS_0_DASHBOARD*/, 0, COLOR_APP, 0xFFFFFF, io_seproxyhal_touch_exit, NULL, NULL},

  {{BAGL_LABELINE                       , 0x00,  30, 106, 320,  30, 0, 0, BAGL_FILL, 0x999999, COLOR_BG_1, BAGL_FONT_OPEN_SANS_SEMIBOLD_8_11PX, 0   }, "VALUE", 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_LABELINE                       , 0x10,  30, 136, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x11,  30, 159, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x12,  30, 182, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x13,  30, 205, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x14,  30, 228, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x15,  30, 251, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x16,  30, 274, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x17,  30, 297, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x18,  30, 320, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  //"..." at the end if too much
  {{BAGL_LABELINE                       , 0x19,  30, 343, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_LABELINE                       , 0x00,   0, 450, 320,  14, 0, 0, 0        , 0x999999, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_8_11PX|BAGL_FONT_ALIGNMENT_CENTER, 0   }, "Review the whole value before continuing.", 10, 0, COLOR_BG_1, NULL, NULL, NULL },
};

const bagl_element_t* ui_details_blue_prepro(const bagl_element_t* element) {
  copy_element_and_map_coin_colors(element);
  if (element->component.userid == 1) {
    tmp_element.text = ui_details_title;
    return &tmp_element;
  }
  else if(element->component.userid > 0) {
    unsigned int length = strlen(ui_details_content);
    if (length >= (element->component.userid & 0xF) * MAX_CHAR_PER_LINE) {
      os_memset(addressSummary, 0, MAX_CHAR_PER_LINE+1);
      os_memmove(addressSummary, ui_details_content+(element->component.userid & 0xF) * MAX_CHAR_PER_LINE, MIN(length - (element->component.userid & 0xF) * MAX_CHAR_PER_LINE, MAX_CHAR_PER_LINE));
      return &tmp_element;
    }
    // nothing to draw for this line
    return 0;
  }
  return &tmp_element;
}

unsigned int ui_details_blue_button(unsigned int button_mask, unsigned int button_mask_counter) {
  return 0;
}

void ui_details_init(const char* title, const char* content, callback_t back_callback) {
  ui_details_title = title;
  ui_details_content = content;
  ui_details_back_callback = back_callback;
  UX_DISPLAY(ui_details_blue, ui_details_blue_prepro);
}

void ui_approval_blue_init(void);

bagl_element_callback_t ui_approval_blue_ok;
bagl_element_callback_t ui_approval_blue_cancel;

const bagl_element_t* ui_approval_blue_ok_callback(const bagl_element_t* e) {
  return ui_approval_blue_ok(e);
}

const bagl_element_t* ui_approval_blue_cancel_callback(const bagl_element_t* e) {
  return ui_approval_blue_cancel(e);
}

typedef enum {
  APPROVAL_TRANSACTION,    
  APPROVAL_MESSAGE, 
} ui_approval_blue_state_t;
ui_approval_blue_state_t G_ui_approval_blue_state;
// pointer to value to be displayed
const char* ui_approval_blue_values[3];
// variable part of the structure
const char* const ui_approval_blue_details_name[][5] = {
  /*APPROVAL_TRANSACTION*/    
  {"AMOUNT",  "ADDRESS", "MAX FEES","CONFIRM TRANSACTION","Transaction details",},
  
  /*APPROVAL_MESSAGE*/ 
  {"HASH",    NULL,      NULL,      "SIGN MESSAGE",       "Message signature", },  
};

const bagl_element_t* ui_approval_blue_1_details(const bagl_element_t* e) {
  if (strlen(ui_approval_blue_values[0])*BAGL_FONT_OPEN_SANS_LIGHT_16_22PX_AVG_WIDTH >= 160) {
    // display details screen
    ui_details_init(ui_approval_blue_details_name[G_ui_approval_blue_state][0], ui_approval_blue_values[0], ui_approval_blue_init);
  }
  return 0;
};

const bagl_element_t* ui_approval_blue_2_details(const bagl_element_t* e) {
  if (strlen(ui_approval_blue_values[1])*BAGL_FONT_OPEN_SANS_REGULAR_10_13PX_AVG_WIDTH >= 160) {
    ui_details_init(ui_approval_blue_details_name[G_ui_approval_blue_state][1], ui_approval_blue_values[1], ui_approval_blue_init);
  }
  return 0;
};

const bagl_element_t* ui_approval_blue_3_details(const bagl_element_t* e) {
  if (strlen(ui_approval_blue_values[2])*BAGL_FONT_OPEN_SANS_REGULAR_10_13PX_AVG_WIDTH >= 160) {
    ui_details_init(ui_approval_blue_details_name[G_ui_approval_blue_state][2], ui_approval_blue_values[2], ui_approval_blue_init);
  }
  return 0;
};

const bagl_element_t ui_approval_blue[] = {
  {{BAGL_RECTANGLE                      , 0x00,   0,  68, 320, 413, 0, 0, BAGL_FILL, COLOR_BG_1, 0x000000, 0                                                                                 , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL },

  // erase screen (only under the status bar)
  {{BAGL_RECTANGLE                      , 0x00,   0,  20, 320,  48, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP, 0                                                      , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL},

  /// TOP STATUS BAR
  {{BAGL_LABELINE                       , 0x60,   0,  45, 320,  30, 0, 0, BAGL_FILL, 0xFFFFFF, COLOR_APP, BAGL_FONT_OPEN_SANS_SEMIBOLD_10_13PX|BAGL_FONT_ALIGNMENT_CENTER, 0   }, NULL, 0, 0, 0, NULL, NULL, NULL},

  // BADGE_TRANSACTION.GIF
  {{BAGL_ICON                           , 0x40,  30,  98,  50,  50, 0, 0, BAGL_FILL, 0       , COLOR_BG_1, 0                                                                                 , 0  } , &C_badge_transaction, 0, 0, 0, NULL, NULL, NULL },

  {{BAGL_LABELINE                       , 0x50, 100, 117, 320,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, NULL, 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_LABELINE                       , 0x00, 100, 138, 320,  30, 0, 0, BAGL_FILL, 0x999999, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_8_11PX, 0   }, "Check and confirm values", 0, 0, 0, NULL, NULL, NULL},


  {{BAGL_LABELINE                       , 0x70,  30, 196, 100,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_SEMIBOLD_8_11PX, 0   }, NULL, 0, 0, 0, NULL, NULL, NULL}, // AMOUNT
  // x-18 when ...
  {{BAGL_LABELINE                       , 0x10, 130, 200, 160,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_LIGHT_16_22PX|BAGL_FONT_ALIGNMENT_RIGHT, 0   }, NULL, 0, 0, 0, NULL, NULL, NULL}, // fullAmount
  {{BAGL_LABELINE                       , 0x20, 284, 196,   6,  16, 0, 0, BAGL_FILL, 0x999999, COLOR_BG_1, BAGL_FONT_SYMBOLS_0|BAGL_FONT_ALIGNMENT_RIGHT, 0   }, BAGL_FONT_SYMBOLS_0_MINIRIGHT, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_NONE   | BAGL_FLAG_TOUCHABLE   , 0x00,   0, 168, 320,  48, 0, 9, BAGL_FILL, 0xFFFFFF, 0x000000, 0                                                                                        , 0   }, NULL, 0, 0xEEEEEE, 0x000000, ui_approval_blue_1_details, ui_menu_item_out_over, ui_menu_item_out_over },
  {{BAGL_RECTANGLE                      , 0x20,   0, 168,   5,  48, 0, 0, BAGL_FILL, COLOR_BG_1, COLOR_BG_1, 0                                                                                    , 0   }, NULL, 0, 0x41CCB4, 0, NULL, NULL, NULL },
  
  {{BAGL_RECTANGLE                      , 0x31,  30, 216, 260,   1, 1, 0, 0        , 0xEEEEEE, COLOR_BG_1, 0                                                                                    , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL },


  {{BAGL_LABELINE                       , 0x71,  30, 245, 100,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_SEMIBOLD_8_11PX, 0   }, NULL, 0, 0, 0, NULL, NULL, NULL}, // ADDRESS
  // x-18 when ...
  {{BAGL_LABELINE                       , 0x11, 130, 245, 160,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX|BAGL_FONT_ALIGNMENT_RIGHT, 0   }, NULL, 0, 0, 0, NULL, NULL, NULL}, // fullAddress
  {{BAGL_LABELINE                       , 0x21, 284, 245,   6,  16, 0, 0, BAGL_FILL, 0x999999, COLOR_BG_1, BAGL_FONT_SYMBOLS_0|BAGL_FONT_ALIGNMENT_RIGHT, 0   }, BAGL_FONT_SYMBOLS_0_MINIRIGHT, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_NONE   | BAGL_FLAG_TOUCHABLE   , 0x00,   0, 217, 320,  48, 0, 9, BAGL_FILL, 0xFFFFFF, 0x000000, 0                                                                                        , 0   }, NULL, 0, 0xEEEEEE, 0x000000, ui_approval_blue_2_details, ui_menu_item_out_over, ui_menu_item_out_over },
  {{BAGL_RECTANGLE                      , 0x21,   0, 217,   5,  48, 0, 0, BAGL_FILL, COLOR_BG_1, COLOR_BG_1, 0                                                                                    , 0   }, NULL, 0, 0x41CCB4, 0, NULL, NULL, NULL },  

  {{BAGL_RECTANGLE                      , 0x32,  30, 265, 260,   1, 1, 0, 0        , 0xEEEEEE, COLOR_BG_1, 0                                                                                    , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL },
  

  {{BAGL_LABELINE                       , 0x72,  30, 294, 100,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_SEMIBOLD_8_11PX, 0   }, NULL, 0, 0, 0, NULL, NULL, NULL}, // MAX FEES
  // x-18 when ...
  {{BAGL_LABELINE                       , 0x12, 130, 294, 160,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX|BAGL_FONT_ALIGNMENT_RIGHT, 0   }, NULL, 0, 0, 0, NULL, NULL, NULL}, //maxFee
  {{BAGL_LABELINE                       , 0x22, 284, 294,   6,  16, 0, 0, BAGL_FILL, 0x999999, COLOR_BG_1, BAGL_FONT_SYMBOLS_0|BAGL_FONT_ALIGNMENT_RIGHT, 0   }, BAGL_FONT_SYMBOLS_0_MINIRIGHT, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_NONE   | BAGL_FLAG_TOUCHABLE   , 0x00,   0, 266, 320,  48, 0, 9, BAGL_FILL, 0xFFFFFF, 0x000000, 0                                                                                        , 0   }, NULL, 0, 0xEEEEEE, 0x000000, ui_approval_blue_3_details, ui_menu_item_out_over, ui_menu_item_out_over },
  {{BAGL_RECTANGLE                      , 0x22,   0, 266,   5,  48, 0, 0, BAGL_FILL, COLOR_BG_1, COLOR_BG_1, 0                                                                                    , 0   }, NULL, 0, 0x41CCB4, 0, NULL, NULL, NULL },

  {{BAGL_RECTANGLE, 0x90, 30, 314, 260, 1, 1, 0, 0, 0xEEEEEE, COLOR_BG_1, 0, 0}, NULL, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE, 0x90, 30, 343, 120, 30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_SEMIBOLD_8_11PX, 0}, "CONTRACT DATA", 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE, 0x90, 133, 343, 140, 30, 0, 0, BAGL_FILL, 0x666666, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX | BAGL_FONT_ALIGNMENT_RIGHT, 0}, "Present", 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_ICON, 0x90, 278, 333, 12, 12, 0, 0, BAGL_FILL, 0, COLOR_BG_1, 0, 0}, &C_icon_warning, 0, 0, 0, NULL, NULL, NULL},     

  {{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00,  40, 414, 115,  36, 0,18, BAGL_FILL, 0xCCCCCC, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_11_14PX|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, "REJECT", 0, 0xB7B7B7, COLOR_BG_1, ui_approval_blue_cancel_callback, NULL, NULL},
  {{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00, 165, 414, 115,  36, 0,18, BAGL_FILL, 0x41ccb4, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_11_14PX|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, "CONFIRM", 0, 0x3ab7a2, COLOR_BG_1, ui_approval_blue_ok_callback, NULL, NULL},

};

const bagl_element_t* ui_approval_blue_prepro(const bagl_element_t* element) {
    copy_element_and_map_coin_colors(element);
    if (element->component.userid == 0) {
      return &tmp_element;
    }
    // none elements are skipped
    if ((element->component.type&(~BAGL_FLAG_TOUCHABLE)) == BAGL_NONE) {
      return 0;
    }
    else {
      switch(element->component.userid&0xF0) {

        // icon
        case 0x40:
          return &tmp_element; 
          break;
		  
        // TITLE
        case 0x60: 
          tmp_element.text = ui_approval_blue_details_name[G_ui_approval_blue_state][3];
          return &tmp_element;
          break;
		  
        // SUBLINE
        case 0x50: 
          tmp_element.text = ui_approval_blue_details_name[G_ui_approval_blue_state][4];
          return &tmp_element;

        // details label
        case 0x70:
          if (!ui_approval_blue_details_name[G_ui_approval_blue_state][element->component.userid&0xF]) {
            return NULL;
          }
          tmp_element.text = ui_approval_blue_details_name[G_ui_approval_blue_state][element->component.userid&0xF];
          return &tmp_element;

        // detail value
        case 0x10:
          // won't display
          if (!ui_approval_blue_details_name[G_ui_approval_blue_state][element->component.userid&0xF]) {
            return NULL;
          }
          // always display the value
          tmp_element.text = ui_approval_blue_values[(element->component.userid&0xF)];

          // x -= 18 when overflow is detected
          if (strlen(ui_approval_blue_values[(element->component.userid&0xF)])*BAGL_FONT_OPEN_SANS_LIGHT_16_22PX_AVG_WIDTH >= 160) {
            tmp_element.component.x -= 18;
          }
          return &tmp_element;
          break;

        // right arrow and left selection rectangle
        case 0x20:
          if (!ui_approval_blue_details_name[G_ui_approval_blue_state][element->component.userid&0xF]) {
            return NULL;
          }
          if (strlen(ui_approval_blue_values[(element->component.userid&0xF)])*BAGL_FONT_OPEN_SANS_LIGHT_16_22PX_AVG_WIDTH < 160) {
            return NULL;
          }

        // horizontal delimiter
        case 0x30:
          return ui_approval_blue_details_name[G_ui_approval_blue_state][element->component.userid&0xF]!=NULL?&tmp_element:NULL;

        case 0x90:
          return (dataPresent && !N_storage.contractDetails);
      }
    }
    return &tmp_element;
}
unsigned int ui_approval_blue_button(unsigned int button_mask, unsigned int button_mask_counter) {
  return 0;
}

#endif // #if defined(TARGET_BLUE)

#if defined(TARGET_BLUE)
const bagl_element_t ui_address_blue[] = {
  {{BAGL_RECTANGLE                      , 0x00,   0,  68, 320, 413, 0, 0, BAGL_FILL, COLOR_BG_1, 0x000000, 0                                                                                 , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL },


  // erase screen (only under the status bar)
  {{BAGL_RECTANGLE                      , 0x00,   0,  20, 320,  48, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP, 0                                                      , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL},

  /// TOP STATUS BAR
  {{BAGL_LABELINE                       , 0x00,   0,  45, 320,  30, 0, 0, BAGL_FILL, 0xFFFFFF, COLOR_APP, BAGL_FONT_OPEN_SANS_SEMIBOLD_10_13PX|BAGL_FONT_ALIGNMENT_CENTER, 0   }, "CONFIRM ACCOUNT", 0, 0, 0, NULL, NULL, NULL},

  //{{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00, 264,  19,  56,  44, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP_LIGHT, BAGL_FONT_SYMBOLS_0|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, " " /*BAGL_FONT_SYMBOLS_0_DASHBOARD*/, 0, COLOR_APP, 0xFFFFFF, io_seproxyhal_touch_exit, NULL, NULL},

  {{BAGL_LABELINE                       , 0x00,  30, 106, 320,  30, 0, 0, BAGL_FILL, 0x999999, COLOR_BG_1, BAGL_FONT_OPEN_SANS_SEMIBOLD_8_11PX, 0   }, "ACCOUNT", 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_LABELINE                       , 0x10,  30, 136, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x11,  30, 159, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00,  40, 414, 115,  36, 0,18, BAGL_FILL, 0xCCCCCC, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_11_14PX|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, "REJECT", 0, 0xB7B7B7, COLOR_BG_1, io_seproxyhal_touch_address_cancel, NULL, NULL},
  {{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00, 165, 414, 115,  36, 0,18, BAGL_FILL, 0x41ccb4, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_11_14PX|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, "CONFIRM", 0, 0x3ab7a2, COLOR_BG_1, io_seproxyhal_touch_address_ok, NULL, NULL},
};

unsigned int ui_address_blue_prepro(const bagl_element_t* element) {
  copy_element_and_map_coin_colors(element);
  if(element->component.userid > 0) {
    unsigned int length = strlen(strings.common.fullAddress);
    if (length >= (element->component.userid & 0xF) * MAX_CHAR_PER_LINE) {
      os_memset(addressSummary, 0, MAX_CHAR_PER_LINE+1);
      os_memmove(addressSummary, strings.common.fullAddress+(element->component.userid & 0xF) * MAX_CHAR_PER_LINE, MIN(length - (element->component.userid & 0xF) * MAX_CHAR_PER_LINE, MAX_CHAR_PER_LINE));
      return &tmp_element;
    }
    // nothing to draw for this line
    return 0;
  }
  return &tmp_element;
}

unsigned int ui_address_blue_button(unsigned int button_mask, unsigned int button_mask_counter) {
  return 0;
}
#endif // #if defined(TARGET_BLUE)

#if defined(TARGET_NANOS)
const bagl_element_t ui_address_nanos[] = {
  // type                               userid    x    y   w    h  str rad fill      fg        bg      fid iid  txt   touchparams...       ]
  {{BAGL_RECTANGLE                      , 0x00,   0,   0, 128,  32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF, 0, 0}, NULL, 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_ICON                           , 0x00,   3,  12,   7,   7, 0, 0, 0        , 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_CROSS  }, NULL, 0, 0, 0, NULL, NULL, NULL },
  {{BAGL_ICON                           , 0x00, 117,  13,   8,   6, 0, 0, 0        , 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_CHECK  }, NULL, 0, 0, 0, NULL, NULL, NULL },

  //{{BAGL_ICON                           , 0x01,  31,   9,  14,  14, 0, 0, 0        , 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_EYE_BADGE  }, NULL, 0, 0, 0, NULL, NULL, NULL },  
  {{BAGL_LABELINE                       , 0x01,   0,  12, 128,  12, 0, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, "Confirm", 0, 0, 0, NULL, NULL, NULL },
  {{BAGL_LABELINE                       , 0x01,   0,  26, 128,  12, 0, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, "address", 0, 0, 0, NULL, NULL, NULL },

  {{BAGL_LABELINE                       , 0x02,   0,  12, 128,  12, 0, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_REGULAR_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, "Address", 0, 0, 0, NULL, NULL, NULL },
  {{BAGL_LABELINE                       , 0x02,  23,  26,  82,  12, 0x80|10, 0, 0  , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px|BAGL_FONT_ALIGNMENT_CENTER, 26  }, (char*)strings.common.fullAddress, 0, 0, 0, NULL, NULL, NULL },
};

unsigned int ui_address_prepro(const bagl_element_t* element) {
    if (element->component.userid > 0) {
        unsigned int display = (ux_step == element->component.userid-1);
        if(display) {
          switch(element->component.userid) {
          case 1:
            UX_CALLBACK_SET_INTERVAL(2000);
            break;
          case 2:
            UX_CALLBACK_SET_INTERVAL(MAX(3000, 1000+bagl_label_roundtrip_duration_ms(element, 7)));
            break;
          }
        }
        return display;
    }
    return 1;
}

unsigned int ui_address_nanos_button(unsigned int button_mask, unsigned int button_mask_counter);
#endif // #if defined(TARGET_NANOS)



#if defined(TARGET_NANOS)
const bagl_element_t ui_approval_nanos[] = {
  // type                               userid    x    y   w    h  str rad fill      fg        bg      fid iid  txt   touchparams...       ]
  {{BAGL_RECTANGLE                      , 0x00,   0,   0, 128,  32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF, 0, 0}, NULL, 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_ICON                           , 0x00,   3,  12,   7,   7, 0, 0, 0        , 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_CROSS  }, NULL, 0, 0, 0, NULL, NULL, NULL },
  {{BAGL_ICON                           , 0x00, 117,  13,   8,   6, 0, 0, 0        , 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_CHECK  }, NULL, 0, 0, 0, NULL, NULL, NULL },

  //{{BAGL_ICON                           , 0x01,  21,   9,  14,  14, 0, 0, 0        , 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_TRANSACTION_BADGE  }, NULL, 0, 0, 0, NULL, NULL, NULL },  
  {{BAGL_LABELINE                       , 0x01,   0,  12, 128,  32, 0, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, "Confirm", 0, 0, 0, NULL, NULL, NULL },
  {{BAGL_LABELINE                       , 0x01,   0,  26, 128,  32, 0, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, "transaction", 0, 0, 0, NULL, NULL, NULL },

  {{BAGL_LABELINE, 0x02, 0, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0}, "WARNING", 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE, 0x02, 23, 26, 82, 12, 0, 0, 0, 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0}, "Data present", 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_LABELINE                       , 0x03,   0,  12, 128,  32, 0, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_REGULAR_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, "Amount", 0, 0, 0, NULL, NULL, NULL },
  {{BAGL_LABELINE                       , 0x03,  23,  26,  82,  12, 0x80|10, 0, 0  , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px|BAGL_FONT_ALIGNMENT_CENTER, 26  }, (char*)strings.common.fullAmount, 0, 0, 0, NULL, NULL, NULL },

  {{BAGL_LABELINE                       , 0x04,   0,  12, 128,  32, 0, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_REGULAR_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, "Address", 0, 0, 0, NULL, NULL, NULL },
  {{BAGL_LABELINE                       , 0x04,  23,  26,  82,  12, 0x80|10, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px|BAGL_FONT_ALIGNMENT_CENTER, 50   }, (char*)strings.common.fullAddress, 0, 0, 0, NULL, NULL, NULL },

  {{BAGL_LABELINE                       , 0x05,   0,  12, 128,  32, 0, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_REGULAR_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, "Maximum fees", 0, 0, 0, NULL, NULL, NULL },
  {{BAGL_LABELINE                       , 0x05,  23,  26,  82,  12, 0x80|10, 0, 0  , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px|BAGL_FONT_ALIGNMENT_CENTER, 26  }, (char*)strings.common.maxFee, 0, 0, 0, NULL, NULL, NULL },

};

unsigned int ui_approval_prepro(const bagl_element_t* element) {
    unsigned int display = 1;
    if (element->component.userid > 0) {
        display = (ux_step == element->component.userid-1);
        if(display) {
          switch(element->component.userid) {
          case 1:
            UX_CALLBACK_SET_INTERVAL(2000);
            break;
          case 2:
            if (dataPresent && !N_storage.contractDetails) {
              UX_CALLBACK_SET_INTERVAL(3000);
            }
            else {
              display = 0;
              ux_step++; // display the next step
            }
            break;
          case 3:
            UX_CALLBACK_SET_INTERVAL(MAX(3000, 1000+bagl_label_roundtrip_duration_ms(element, 7)));
            break;
          case 4:
            UX_CALLBACK_SET_INTERVAL(MAX(3000, 1000+bagl_label_roundtrip_duration_ms(element, 7)));
            break;
          case 5:
            UX_CALLBACK_SET_INTERVAL(MAX(3000, 1000+bagl_label_roundtrip_duration_ms(element, 7)));
            break;
          }
        }
    }
    return display;
}

unsigned int ui_approval_nanos_button(unsigned int button_mask, unsigned int button_mask_counter);

const bagl_element_t ui_approval_signMessage_nanos[] = {
  // type                               userid    x    y   w    h  str rad fill      fg        bg      fid iid  txt   touchparams...       ]
  {{BAGL_RECTANGLE                      , 0x00,   0,   0, 128,  32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF, 0, 0}, NULL, 0, 0, 0, NULL, NULL, NULL},
 
  {{BAGL_ICON                           , 0x00,   3,  12,   7,   7, 0, 0, 0        , 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_CROSS  }, NULL, 0, 0, 0, NULL, NULL, NULL },
  {{BAGL_ICON                           , 0x00, 117,  13,   8,   6, 0, 0, 0        , 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_CHECK  }, NULL, 0, 0, 0, NULL, NULL, NULL },

  //{{BAGL_ICON                           , 0x01,  28,   9,  14,  14, 0, 0, 0        , 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_TRANSACTION_BADGE  }, NULL, 0, 0, 0, NULL, NULL, NULL },
  {{BAGL_LABELINE                       , 0x01,   0,  12, 128,  32, 0, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, "Sign the", 0, 0, 0, NULL, NULL, NULL },
  {{BAGL_LABELINE                       , 0x01,   0,  26, 128,  32, 0, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, "message", 0, 0, 0, NULL, NULL, NULL },

  {{BAGL_LABELINE                       , 0x02,   0,  12, 128,  32, 0, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_REGULAR_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, "Message hash", 0, 0, 0, NULL, NULL, NULL },
  {{BAGL_LABELINE                       , 0x02,   0,  26, 128,  32, 0, 0, 0        , 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px|BAGL_FONT_ALIGNMENT_CENTER, 0  }, strings.common.fullAddress, 0, 0, 0, NULL, NULL, NULL },

};

unsigned int
ui_approval_signMessage_nanos_button(unsigned int button_mask, unsigned int button_mask_counter);

unsigned int ui_approval_signMessage_prepro(const bagl_element_t *element) {
    if (element->component.userid > 0) {
        switch (element->component.userid) {
        case 1:
            UX_CALLBACK_SET_INTERVAL(2000);
            break;
        case 2:
            UX_CALLBACK_SET_INTERVAL(3000);
            break;
        }
        return (ux_step == element->component.userid - 1);
    }
    return 1;
}

#endif // #if defined(TARGET_NANOS)

#if defined(TARGET_BLUE)
const bagl_element_t ui_data_selector_blue[] = {
  {{BAGL_RECTANGLE                      , 0x00,   0,  68, 320, 413, 0, 0, BAGL_FILL, COLOR_BG_1, 0x000000, 0                                                                                 , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL },


  // erase screen (only under the status bar)
  {{BAGL_RECTANGLE                      , 0x00,   0,  20, 320,  48, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP, 0                                                      , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL},

  /// TOP STATUS BAR
  {{BAGL_LABELINE                       , 0x00,   0,  45, 320,  30, 0, 0, BAGL_FILL, 0xFFFFFF, COLOR_APP, BAGL_FONT_OPEN_SANS_SEMIBOLD_10_13PX|BAGL_FONT_ALIGNMENT_CENTER, 0   }, "CONFIRM SELECTOR", 0, 0, 0, NULL, NULL, NULL},

  //{{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00, 264,  19,  56,  44, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP_LIGHT, BAGL_FONT_SYMBOLS_0|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, " " /*BAGL_FONT_SYMBOLS_0_DASHBOARD*/, 0, COLOR_APP, 0xFFFFFF, io_seproxyhal_touch_exit, NULL, NULL},

  {{BAGL_LABELINE                       , 0x00,  30, 106, 320,  30, 0, 0, BAGL_FILL, 0x999999, COLOR_BG_1, BAGL_FONT_OPEN_SANS_SEMIBOLD_8_11PX, 0   }, "SELECTOR", 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_LABELINE                       , 0x10,  30, 136, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, strings.tmp.tmp, 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00,  40, 414, 115,  36, 0,18, BAGL_FILL, 0xCCCCCC, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_11_14PX|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, "REJECT", 0, 0xB7B7B7, COLOR_BG_1, io_seproxyhal_touch_data_cancel, NULL, NULL},
  {{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00, 165, 414, 115,  36, 0,18, BAGL_FILL, 0x41ccb4, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_11_14PX|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, "CONFIRM", 0, 0x3ab7a2, COLOR_BG_1, io_seproxyhal_touch_data_ok, NULL, NULL},
};

unsigned int ui_data_selector_blue_prepro(const bagl_element_t* element) {
  copy_element_and_map_coin_colors(element);
  if(element->component.userid > 0) {
    unsigned int length = strlen(strings.tmp.tmp);
    unsigned int offset = (element->component.userid & 0xF) * 24;
    if (length >= offset) {
      unsigned int copyLength = ((offset + 24) > length ? length - offset : 24);
      os_memset(addressSummary, 0, 25);
      os_memmove(addressSummary, strings.tmp.tmp + offset, copyLength);
      return &tmp_element;
    }
    // nothing to draw for this line
    return 0;
  }
  return &tmp_element;
}

unsigned int ui_data_selector_blue_button(unsigned int button_mask, unsigned int button_mask_counter) {
  return 0;
}
#endif // #if defined(TARGET_BLUE)


#if defined(TARGET_NANOS)
const bagl_element_t ui_data_selector_nanos[] = {
    // type                               userid    x    y   w    h  str rad
    // fill      fg        bg      fid iid  txt   touchparams...       ]
    {{BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF, 0, 0}, NULL, 0, 0, 0, NULL, NULL, NULL},
    {{BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_CROSS}, NULL, 0, 0, 0, NULL, NULL, NULL},
    {{BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_CHECK}, NULL, 0, 0, 0, NULL, NULL, NULL},
    {{BAGL_LABELINE, 0x01, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0}, "Confirm", 0, 0, 0, NULL, NULL, NULL},
    {{BAGL_LABELINE, 0x01, 0, 26, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0}, "selector", 0, 0, 0, NULL, NULL, NULL},

    {{BAGL_LABELINE, 0x02, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0}, "Selector", 0, 0, 0, NULL, NULL, NULL},
    {{BAGL_LABELINE, 0x02, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26}, (char *)strings.tmp.tmp, 0, 0, 0, NULL, NULL, NULL},
};

unsigned int ui_data_selector_prepro(const bagl_element_t *element) {
    if (element->component.userid > 0) {
        unsigned int display = (ux_step == element->component.userid - 1);
        if (display) {
            switch (element->component.userid) {
                case 1:
                    UX_CALLBACK_SET_INTERVAL(2000);
                    break;
                case 2:
                    UX_CALLBACK_SET_INTERVAL(MAX(
                        3000, 1000 + bagl_label_roundtrip_duration_ms(element, 7)));
                    break;
            }
        }
        return display;
    }
    return 1;
}

unsigned int ui_data_selector_nanos_button(unsigned int button_mask,
                                     unsigned int button_mask_counter);
#endif // #if defined(TARGET_NANOS)

#if defined(TARGET_BLUE)
const bagl_element_t ui_data_parameter_blue[] = {
  {{BAGL_RECTANGLE                      , 0x00,   0,  68, 320, 413, 0, 0, BAGL_FILL, COLOR_BG_1, 0x000000, 0                                                                                 , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL },


  // erase screen (only under the status bar)
  {{BAGL_RECTANGLE                      , 0x00,   0,  20, 320,  48, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP, 0                                                      , 0   }, NULL, 0, 0, 0, NULL, NULL, NULL},

  /// TOP STATUS BAR
  {{BAGL_LABELINE                       , 0x00,   0,  45, 320,  30, 0, 0, BAGL_FILL, 0xFFFFFF, COLOR_APP, BAGL_FONT_OPEN_SANS_SEMIBOLD_10_13PX|BAGL_FONT_ALIGNMENT_CENTER, 0   }, "CONFIRM PARAMETER", 0, 0, 0, NULL, NULL, NULL},

  //{{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00, 264,  19,  56,  44, 0, 0, BAGL_FILL, COLOR_APP, COLOR_APP_LIGHT, BAGL_FONT_SYMBOLS_0|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, " " /*BAGL_FONT_SYMBOLS_0_DASHBOARD*/, 0, COLOR_APP, 0xFFFFFF, io_seproxyhal_touch_exit, NULL, NULL},

  {{BAGL_LABELINE                       , 0x00,  30, 106, 320,  30, 0, 0, BAGL_FILL, 0x999999, COLOR_BG_1, BAGL_FONT_OPEN_SANS_SEMIBOLD_8_11PX, 0   }, "PARAMETER", 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_LABELINE                       , 0x00,  30, 136, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, strings.tmp.tmp2, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x10,  30, 159, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x11,  30, 182, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x12,  30, 205, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},
  {{BAGL_LABELINE                       , 0x13,  30, 228, 260,  30, 0, 0, BAGL_FILL, 0x000000, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_10_13PX, 0   }, addressSummary, 0, 0, 0, NULL, NULL, NULL},

  {{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00,  40, 414, 115,  36, 0,18, BAGL_FILL, 0xCCCCCC, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_11_14PX|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, "REJECT", 0, 0xB7B7B7, COLOR_BG_1, io_seproxyhal_touch_data_cancel, NULL, NULL},
  {{BAGL_RECTANGLE | BAGL_FLAG_TOUCHABLE, 0x00, 165, 414, 115,  36, 0,18, BAGL_FILL, 0x41ccb4, COLOR_BG_1, BAGL_FONT_OPEN_SANS_REGULAR_11_14PX|BAGL_FONT_ALIGNMENT_CENTER|BAGL_FONT_ALIGNMENT_MIDDLE, 0 }, "CONFIRM", 0, 0x3ab7a2, COLOR_BG_1, io_seproxyhal_touch_data_ok, NULL, NULL},
};

int local_strchr(char *string, char ch) {
    unsigned int length = strlen(string);
    unsigned int i;
    for (i=0; i<length; i++) {
        if (string[i] == ch) {
            return i;
        }
    }
    return -1;
}

unsigned int ui_data_parameter_blue_prepro(const bagl_element_t* element) {
  copy_element_and_map_coin_colors(element);
  if(element->component.userid > 0) {
    unsigned int pos = (element->component.userid & 0xF);
    unsigned int i;
    unsigned int offset = 0;
    unsigned int copyLength;
    for (i=0; i<pos; i++) {
        offset += local_strchr(strings.tmp.tmp + offset, ':');
        if (offset < 0) {
            THROW(EXCEPTION);
        }
        offset = offset + 1;
    }
    if (pos == 3) {
        copyLength = strlen(strings.tmp.tmp) - offset;
    }
    else {
        unsigned int endOffset;
        endOffset = offset + local_strchr(strings.tmp.tmp + offset, ':');
        copyLength = endOffset - offset;
    }
    os_memmove(addressSummary, strings.tmp.tmp + offset, copyLength);
    addressSummary[copyLength] = '\0';
  }
  return &tmp_element;
}

unsigned int ui_data_parameter_blue_button(unsigned int button_mask, unsigned int button_mask_counter) {
  return 0;
}
#endif // #if defined(TARGET_BLUE)


#if defined(TARGET_NANOS)
const bagl_element_t ui_data_parameter_nanos[] = {
    // type                               userid    x    y   w    h  str rad
    // fill      fg        bg      fid iid  txt   touchparams...       ]
    {{BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF, 0, 0}, NULL, 0, 0, 0, NULL, NULL, NULL},

    {{BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_CROSS}, NULL, 0, 0, 0, NULL, NULL, NULL},
    {{BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_CHECK}, NULL, 0, 0, 0, NULL, NULL, NULL},

    {{BAGL_LABELINE, 0x01, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0}, "Confirm", 0, 0, 0, NULL, NULL, NULL},
    {{BAGL_LABELINE, 0x01, 0, 26, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0}, "parameter", 0, 0, 0, NULL, NULL, NULL},

    {{BAGL_LABELINE, 0x02, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0}, (char*)strings.tmp.tmp2, 0, 0, 0, NULL, NULL, NULL},
    {{BAGL_LABELINE, 0x02, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26}, (char *)strings.tmp.tmp, 0, 0, 0, NULL, NULL, NULL},
};

unsigned int ui_data_parameter_prepro(const bagl_element_t *element) {
    if (element->component.userid > 0) {
        unsigned int display = (ux_step == element->component.userid - 1);
        if (display) {
            switch (element->component.userid) {
            case 1:
                UX_CALLBACK_SET_INTERVAL(2000);
                break;
            case 2:
                UX_CALLBACK_SET_INTERVAL(MAX(
                    3000, 1000 + bagl_label_roundtrip_duration_ms(element, 7)));
                break;
            }
        }
        return display;
    }
    return 1;
}

unsigned int ui_data_parameter_nanos_button(unsigned int button_mask,
                                     unsigned int button_mask_counter);
#endif // #if defined(TARGET_NANOS)


void ui_idle(void) {
#if defined(TARGET_BLUE)
    UX_DISPLAY(ui_idle_blue, ui_idle_blue_prepro);
#elif defined(TARGET_NANOS)
    UX_MENU_DISPLAY(0, menu_main, NULL);   
#endif // #if TARGET_ID
}

#if defined(TARGET_BLUE)
unsigned int io_seproxyhal_touch_settings(const bagl_element_t *e) {
  UX_DISPLAY(ui_settings_blue, ui_settings_blue_prepro);
  return 0; // do not redraw button, screen has switched
}
#endif // #if defined(TARGET_BLUE)

unsigned int io_seproxyhal_touch_exit(const bagl_element_t *e) {
    // Go back to the dashboard
    os_sched_exit(0);
    return 0; // do not redraw the widget
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

#if defined(TARGET_NANOS)
unsigned int ui_address_nanos_button(unsigned int button_mask, unsigned int button_mask_counter) {
    switch(button_mask) {
        case BUTTON_EVT_RELEASED|BUTTON_LEFT: // CANCEL
			      io_seproxyhal_touch_address_cancel(NULL);
            break;

        case BUTTON_EVT_RELEASED|BUTTON_RIGHT: { // OK
			      io_seproxyhal_touch_address_ok(NULL);
			      break;
        }
    }
    return 0;
}
#endif // #if defined(TARGET_NANOS)

uint32_t getV(txContent_t *txContent) {
    uint32_t v = 0;        
    if (txContent->vLength == 1) {
      v = txContent->v[0];
    }
    else 
    if (txContent->vLength == 2) {
      v = (txContent->v[0] << 8) | txContent->v[1];
    }
    else 
    if (txContent->vLength == 3) {
      v = (txContent->v[0] << 16) | (txContent->v[1] << 8) | txContent->v[2];
    }    
    else
    if (txContent->vLength == 4) {
      v = (txContent->v[0] << 24) | (txContent->v[1] << 16) | 
          (txContent->v[2] << 8) | txContent->v[3];
    }
    else 
    if (txContent->vLength != 0) {
        PRINTF("Unexpected v format\n");
        THROW(EXCEPTION);
    }
    return v;

}

void io_seproxyhal_send_status(uint32_t sw) {
    G_io_apdu_buffer[0] = ((sw >> 8) & 0xff);
    G_io_apdu_buffer[1] = (sw & 0xff);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

unsigned int io_seproxyhal_touch_tx_ok(const bagl_element_t *e) {
    uint8_t privateKeyData[32];
    uint8_t signature[100];
    uint8_t signatureLength;
    cx_ecfp_private_key_t privateKey;
    uint32_t tx = 0;
    uint8_t rLength, sLength, rOffset, sOffset;
    uint32_t v = getV(&tmpContent.txContent);
    os_perso_derive_node_bip32(CX_CURVE_256K1, tmpCtx.transactionContext.bip32Path,
                               tmpCtx.transactionContext.pathLength,
                               privateKeyData, NULL);
    cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32,
                                 &privateKey);
    os_memset(privateKeyData, 0, sizeof(privateKeyData));
    unsigned int info = 0;    
    signatureLength =
        cx_ecdsa_sign(&privateKey, CX_RND_RFC6979 | CX_LAST, CX_SHA256,
                      tmpCtx.transactionContext.hash,
                      sizeof(tmpCtx.transactionContext.hash), signature, &info);
    os_memset(&privateKey, 0, sizeof(privateKey));    
    // Parity is present in the sequence tag in the legacy API
    if (tmpContent.txContent.vLength == 0) {
      // Legacy API
      G_io_apdu_buffer[0] = 27;
    }
    else {
      // New API
      // Note that this is wrong for a large v, but the client can always recover
      G_io_apdu_buffer[0] = (v * 2) + 35; 
    }
    if (info & CX_ECCINFO_PARITY_ODD) {
      G_io_apdu_buffer[0]++;
    }
    if (info & CX_ECCINFO_xGTn) {
      G_io_apdu_buffer[0] += 2;
    }
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
send:    
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


unsigned int io_seproxyhal_touch_signMessage_ok(const bagl_element_t *e) {
    uint8_t privateKeyData[32];
    uint8_t signature[100];
    uint8_t signatureLength;
    cx_ecfp_private_key_t privateKey;
    uint32_t tx = 0;
    uint8_t rLength, sLength, rOffset, sOffset;
    os_perso_derive_node_bip32(
        CX_CURVE_256K1, tmpCtx.messageSigningContext.bip32Path,
        tmpCtx.messageSigningContext.pathLength, privateKeyData, NULL);
    cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
    os_memset(privateKeyData, 0, sizeof(privateKeyData));
    unsigned int info = 0;    
    signatureLength =
        cx_ecdsa_sign(&privateKey, CX_RND_RFC6979 | CX_LAST, CX_SHA256,
                      tmpCtx.messageSigningContext.hash,
                      sizeof(tmpCtx.messageSigningContext.hash), signature, &info);
    os_memset(&privateKey, 0, sizeof(privateKey));
    G_io_apdu_buffer[0] = 27;
    if (info & CX_ECCINFO_PARITY_ODD) {
      G_io_apdu_buffer[0]++;
    }
    if (info & CX_ECCINFO_xGTn) {
      G_io_apdu_buffer[0] += 2;
    }    
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

unsigned int io_seproxyhal_touch_signMessage_cancel(const bagl_element_t *e) {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_data_ok(const bagl_element_t *e) {
    parserStatus_e txResult = USTREAM_FINISHED;
    txResult = continueTx(&txContext);
    switch (txResult) {
    case USTREAM_SUSPENDED:
        break;
    case USTREAM_FINISHED:
        break;
    case USTREAM_PROCESSING:
        io_seproxyhal_send_status(0x9000);
        ui_idle();
        break;
    case USTREAM_FAULT:
        io_seproxyhal_send_status(0x6A80);
        ui_idle();
        break;
    default:
        PRINTF("Unexpected parser status\n");
        io_seproxyhal_send_status(0x6A80);
        ui_idle();
    }

    if (txResult == USTREAM_FINISHED) {
        finalizeParsing(false);
    }

    return 0;
}


unsigned int io_seproxyhal_touch_data_cancel(const bagl_element_t *e) {
    io_seproxyhal_send_status(0x6985);
    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget    
}

#if defined(TARGET_BLUE)
void ui_approval_blue_init(void) {
  UX_DISPLAY(ui_approval_blue, ui_approval_blue_prepro);
}

void ui_approval_transaction_blue_init(void) {
  ui_approval_blue_ok = (bagl_element_callback_t) io_seproxyhal_touch_tx_ok;
  ui_approval_blue_cancel = (bagl_element_callback_t) io_seproxyhal_touch_tx_cancel;
  G_ui_approval_blue_state = APPROVAL_TRANSACTION;
  ui_approval_blue_values[0] = strings.common.fullAmount;
  ui_approval_blue_values[1] = strings.common.fullAddress;
  ui_approval_blue_values[2] = strings.common.maxFee;
  ui_approval_blue_init();
}

void ui_approval_message_sign_blue_init(void) {
  ui_approval_blue_ok = (bagl_element_callback_t) io_seproxyhal_touch_signMessage_ok;
  ui_approval_blue_cancel = (bagl_element_callback_t) io_seproxyhal_touch_signMessage_cancel;
  G_ui_approval_blue_state = APPROVAL_MESSAGE;
  ui_approval_blue_values[0] = strings.common.fullAmount;
  ui_approval_blue_values[1] = NULL;
  ui_approval_blue_values[2] = NULL;
  ui_approval_blue_init();
}

#elif defined(TARGET_NANOS)
unsigned int ui_approval_nanos_button(unsigned int button_mask, unsigned int button_mask_counter) {
    switch(button_mask) {
        case BUTTON_EVT_RELEASED|BUTTON_LEFT:
            io_seproxyhal_touch_tx_cancel(NULL);
            break;

        case BUTTON_EVT_RELEASED|BUTTON_RIGHT: {
			      io_seproxyhal_touch_tx_ok(NULL);
            break;
        }
    }    
    return 0;
}


unsigned int ui_approval_signMessage_nanos_button(unsigned int button_mask, unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        io_seproxyhal_touch_signMessage_cancel(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_RIGHT: {
        io_seproxyhal_touch_signMessage_ok(NULL);
        break;
    }
    }
    return 0;
}

unsigned int ui_data_selector_nanos_button(unsigned int button_mask,
                                     unsigned int button_mask_counter) {
   switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        io_seproxyhal_touch_data_cancel(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_RIGHT: {
        io_seproxyhal_touch_data_ok(NULL);
        break;
    }
    }
    return 0;
}

unsigned int ui_data_parameter_nanos_button(unsigned int button_mask,
                                     unsigned int button_mask_counter) {
   switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        io_seproxyhal_touch_data_cancel(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_RIGHT: {
        io_seproxyhal_touch_data_ok(NULL);
        break;
    }
    }
    return 0;
}

#endif // #if defined(TARGET_NANOS)

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
    if (tmpCtx.publicKeyContext.getChaincode) {
      os_memmove(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.chainCode, 32);
      tx += 32;
    }
    return tx;
}

void convertUint256BE(uint8_t *data, uint32_t length, uint256_t *target) {
    uint8_t tmp[32];
    os_memset(tmp, 0, 32);
    os_memmove(tmp + 32 - length, data, length);
    readu256BE(tmp, target);
}

uint32_t splitBinaryParameterPart(char *result, uint8_t *parameter) {
    uint32_t i;
    for (i=0; i<8; i++) {
        if (parameter[i] != 0x00) {
            break;
        }
    }
    if (i == 8) {
        result[0] = '0';
        result[1] = '0';
        result[2] = '\0';
        return 2;
    }
    else {
        array_hexstr(result, parameter + i, 8 - i);
        return ((8 - i) * 2);
    }
}

tokenDefinition_t* getKnownToken() {
    tokenDefinition_t *currentToken = NULL;
    uint32_t numTokens = 0;
    uint32_t i;
    switch(chainConfig->kind) {
        case CHAIN_KIND_AKROMA:
            numTokens = NUM_TOKENS_AKROMA;
            break;
        case CHAIN_KIND_ETHEREUM:
            numTokens = NUM_TOKENS_ETHEREUM;
            break;
        case CHAIN_KIND_ETHEREUM_CLASSIC:
            numTokens = NUM_TOKENS_ETHEREUM_CLASSIC;
            break;       
        case CHAIN_KIND_PIRL:
            numTokens = NUM_TOKENS_PIRL;
            break;
        case CHAIN_KIND_POA:
            numTokens = NUM_TOKENS_POA;
            break;
        case CHAIN_KIND_RSK:
            numTokens = NUM_TOKENS_RSK;
            break;                      
        case CHAIN_KIND_EXPANSE:
            numTokens = NUM_TOKENS_EXPANSE;
            break;
        case CHAIN_KIND_UBIQ:
            numTokens = NUM_TOKENS_UBIQ;
            break;
        case CHAIN_KIND_WANCHAIN:
            numTokens = NUM_TOKENS_WANCHAIN;
            break;        
        case CHAIN_KIND_KUSD:
            numTokens = NUM_TOKENS_KUSD;
            break;    
        case CHAIN_KIND_MUSICOIN:
            numTokens = NUM_TOKENS_MUSICOIN;
            break;
        case CHAIN_KIND_CALLISTO:
            numTokens = NUM_TOKENS_CALLISTO;
            break;            
        case CHAIN_KIND_ETHERSOCIAL:
            numTokens = NUM_TOKENS_ETHERSOCIAL;
            break;
        case CHAIN_KIND_ELLAISM:
            numTokens = NUM_TOKENS_ELLAISM;
            break;
        case CHAIN_KIND_ETHER1:
            numTokens = NUM_TOKENS_ETHER1;
            break;
        case CHAIN_KIND_ETHERGEM:
            numTokens = NUM_TOKENS_ETHERGEM;
            break;
        case CHAIN_KIND_ATHEIOS:
            numTokens = NUM_TOKENS_ATHEIOS;
            break;
        case CHAIN_KIND_GOCHAIN:
            numTokens = NUM_TOKENS_GOCHAIN;
            break;
        case CHAIN_KIND_EOSCLASSIC:
            numTokens = NUM_TOKENS_EOSCLASSIC;
            break;
        case CHAIN_KIND_MIX:
            numTokens = NUM_TOKENS_MIX;
            break;
        case CHAIN_KIND_AQUACHAIN:
            numTokens = NUM_TOKENS_AQUACHAIN;
            break;
    }
    for (i=0; i<numTokens; i++) {            
        switch(chainConfig->kind) {
            case CHAIN_KIND_AKROMA:
                currentToken = PIC(&TOKENS_AKROMA[i]);
                break;
            case CHAIN_KIND_ETHEREUM:
                currentToken = PIC(&TOKENS_ETHEREUM[i]);
                break;
            case CHAIN_KIND_ETHEREUM_CLASSIC:
                currentToken = PIC(&TOKENS_ETHEREUM_CLASSIC[i]);
                break;                
            case CHAIN_KIND_PIRL:
                currentToken = PIC(&TOKENS_PIRL[i]);
                break;
            case CHAIN_KIND_POA:
                    currentToken = PIC(&TOKENS_POA[i]);
                    break;
            case CHAIN_KIND_RSK:
                currentToken = PIC(&TOKENS_RSK[i]);
                break;                                
            case CHAIN_KIND_EXPANSE:
                currentToken = PIC(&TOKENS_EXPANSE[i]);
                break;              
            case CHAIN_KIND_UBIQ:
                currentToken = PIC(&TOKENS_UBIQ[i]);
                break;              
            case CHAIN_KIND_WANCHAIN:
                currentToken = PIC(&TOKENS_WANCHAIN[i]);
                break;                              
            case CHAIN_KIND_KUSD:
                currentToken = PIC(&TOKENS_KUSD[i]);
                break;
            case CHAIN_KIND_MUSICOIN:
                currentToken = PIC(&TOKENS_MUSICOIN[i]);
                break;
            case CHAIN_KIND_CALLISTO:
                currentToken = PIC(&TOKENS_CALLISTO[i]);
                break;
            case CHAIN_KIND_ETHERSOCIAL:
                currentToken = PIC(&TOKENS_ETHERSOCIAL[i]);
                break;
            case CHAIN_KIND_ELLAISM:
                currentToken = PIC(&TOKENS_ELLAISM[i]);
                break;
            case CHAIN_KIND_ETHER1:
                currentToken = PIC(&TOKENS_ETHER1[i]);
                break;
            case CHAIN_KIND_ETHERGEM:
                currentToken = PIC(&TOKENS_ETHERGEM[i]);
                break;
            case CHAIN_KIND_ATHEIOS:
                currentToken = PIC(&TOKENS_ATHEIOS[i]);
                break;
            case CHAIN_KIND_GOCHAIN:
                currentToken = PIC(&TOKENS_GOCHAIN[i]);
                break;
            case CHAIN_KIND_EOSCLASSIC:
                currentToken = PIC(&TOKENS_EOSCLASSIC[i]);
                break;
            case CHAIN_KIND_MIX:
                currentToken = PIC(&TOKENS_MIX[i]);
                break;
            case CHAIN_KIND_AQUACHAIN:
                currentToken = PIC(&TOKENS_AQUACHAIN[i]);
                break;
        } 
        if (os_memcmp(currentToken->address, tmpContent.txContent.destination, 20) == 0) {
            return currentToken;
        }
    }
    return NULL;
}


customStatus_e customProcessor(txContext_t *context) {
    if ((context->currentField == TX_RLP_DATA) &&
        (context->currentFieldLength != 0)) {
        dataPresent = true;
        // If handling a new contract rather than a function call, abort immediately
        if (tmpContent.txContent.destinationLength == 0) {
            return CUSTOM_NOT_HANDLED;
        }
        if (context->currentFieldPos == 0) {            
            // If handling the beginning of the data field, assume that the function selector is present
            if (context->commandLength < 4) {
                PRINTF("Missing function selector\n");
                return CUSTOM_FAULT;
            }
            // Initial check to see if the token content can be processed
            tokenProvisioned = 
                (context->currentFieldLength == sizeof(dataContext.tokenContext.data)) &&
                (os_memcmp(context->workBuffer, TOKEN_TRANSFER_ID, 4) == 0) && 
                (getKnownToken() != NULL);
        }
        if (tokenProvisioned) {
            if (context->currentFieldPos < context->currentFieldLength) {
                uint32_t copySize = (context->commandLength <
                                        ((context->currentFieldLength -
                                                   context->currentFieldPos))
                                        ? context->commandLength
                                            : context->currentFieldLength -
                                                   context->currentFieldPos);
                copyTxData(context,
                    dataContext.tokenContext.data + context->currentFieldPos,
                    copySize);
            }
            if (context->currentFieldPos == context->currentFieldLength) {
                context->currentField++;
                context->processingField = false;
            }
            return CUSTOM_HANDLED;
        }
        else {
            uint32_t blockSize;
            uint32_t copySize;
            uint32_t fieldPos = context->currentFieldPos;
            if (fieldPos == 0) {
                if (!N_storage.dataAllowed) {
                  PRINTF("Data field forbidden\n");
                  return CUSTOM_FAULT;
                }
                if (!N_storage.contractDetails) {
                  return CUSTOM_NOT_HANDLED;
                }
                dataContext.rawDataContext.fieldIndex = 0;
                dataContext.rawDataContext.fieldOffset = 0;
                blockSize = 4;
            }
            else {
                if (!N_storage.contractDetails) {
                  return CUSTOM_NOT_HANDLED;
                }              
                blockSize = 32 - (dataContext.rawDataContext.fieldOffset % 32);
            }

            // Sanity check
            if ((context->currentFieldLength - fieldPos) < blockSize) {
                PRINTF("Unconsistent data\n");
                return CUSTOM_FAULT;
            }

            copySize = (context->commandLength < blockSize ? context->commandLength : blockSize);
            copyTxData(context,
                        dataContext.rawDataContext.data + dataContext.rawDataContext.fieldOffset,
                        copySize);

            if (context->currentFieldPos == context->currentFieldLength) {
                context->currentField++;
                context->processingField = false;
            }

            dataContext.rawDataContext.fieldOffset += copySize;

            if (copySize == blockSize) {
                // Can display
                if (fieldPos != 0) {
                    dataContext.rawDataContext.fieldIndex++;
                }
                dataContext.rawDataContext.fieldOffset = 0;                    
                if (fieldPos == 0) {
                    array_hexstr(strings.tmp.tmp, dataContext.rawDataContext.data, 4);
#if defined(TARGET_BLUE)
                    UX_DISPLAY(ui_data_selector_blue, ui_data_selector_blue_prepro);
#elif defined(TARGET_NANOS)
                    ux_step = 0;
                    ux_step_count = 2;
                    UX_DISPLAY(ui_data_selector_nanos, ui_data_selector_prepro);
#endif // #if TARGET_ID
                }
                else {
                    uint32_t offset = 0;
                    uint32_t i;
                    snprintf(strings.tmp.tmp2, sizeof(strings.tmp.tmp2), "Field %d", dataContext.rawDataContext.fieldIndex);
                    for (i=0; i<4; i++) {
                        offset += splitBinaryParameterPart(strings.tmp.tmp + offset, dataContext.rawDataContext.data + 8 * i);
                        if (i != 3) {
                            strings.tmp.tmp[offset++] = ':';
                        }
                    }
#if defined(TARGET_BLUE)
                    UX_DISPLAY(ui_data_parameter_blue, ui_data_parameter_blue_prepro);
#elif defined(TARGET_NANOS)
                    ux_step = 0;
                    ux_step_count = 2;
                    UX_DISPLAY(ui_data_parameter_nanos, ui_data_parameter_prepro);
#endif // #if TARGET_ID                        
                }
            }
            else {
                return CUSTOM_HANDLED;
            }

            return CUSTOM_SUSPENDED;            
        }
    }
    return CUSTOM_NOT_HANDLED;
}


void handleGetPublicKey(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
  UNUSED(dataLength);  
  uint8_t privateKeyData[32];
  uint32_t bip32Path[MAX_BIP32_PATH];
  uint32_t i;
  uint8_t bip32PathLength = *(dataBuffer++);
  cx_ecfp_private_key_t privateKey;

  if ((bip32PathLength < 0x01) ||
      (bip32PathLength > MAX_BIP32_PATH)) {
    PRINTF("Invalid path\n");
    THROW(0x6a80);
  }
  if ((p1 != P1_CONFIRM) && (p1 != P1_NON_CONFIRM)) {
    THROW(0x6B00);
  }
  if ((p2 != P2_CHAINCODE) && (p2 != P2_NO_CHAINCODE)) {
    THROW(0x6B00);
  }
  for (i = 0; i < bip32PathLength; i++) {
    bip32Path[i] = (dataBuffer[0] << 24) |
                   (dataBuffer[1] << 16) |
                   (dataBuffer[2] << 8) | (dataBuffer[3]);
    dataBuffer += 4;
  }
  tmpCtx.publicKeyContext.getChaincode = (p2 == P2_CHAINCODE);
  os_perso_derive_node_bip32(CX_CURVE_256K1, bip32Path, bip32PathLength, privateKeyData, (tmpCtx.publicKeyContext.getChaincode ? tmpCtx.publicKeyContext.chainCode : NULL));
  cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
  cx_ecfp_generate_pair(CX_CURVE_256K1, &tmpCtx.publicKeyContext.publicKey, &privateKey, 1);
  os_memset(&privateKey, 0, sizeof(privateKey));
  os_memset(privateKeyData, 0, sizeof(privateKeyData));
  getEthAddressStringFromKey(&tmpCtx.publicKeyContext.publicKey, tmpCtx.publicKeyContext.address, &sha3);
  if (p1 == P1_NON_CONFIRM) {
    *tx = set_result_get_publicKey();
    THROW(0x9000);
  } 
  else 
  {
    /*
    addressSummary[0] = '0';
    addressSummary[1] = 'x';
    os_memmove((unsigned char *)(addressSummary + 2), tmpCtx.publicKeyContext.address, 4);
    os_memmove((unsigned char *)(addressSummary + 6), "...", 3);
    os_memmove((unsigned char *)(addressSummary + 9), tmpCtx.publicKeyContext.address + 40 - 4, 4);
    addressSummary[13] = '\0';
    */

    // prepare for a UI based reply
#if defined(TARGET_BLUE)
    snprintf(strings.common.fullAddress, sizeof(strings.common.fullAddress), "0x%.*s", 40, tmpCtx.publicKeyContext.address);
    UX_DISPLAY(ui_address_blue, ui_address_blue_prepro);
#elif defined(TARGET_NANOS)
    snprintf(strings.common.fullAddress, sizeof(strings.common.fullAddress), "0x%.*s", 40, tmpCtx.publicKeyContext.address);
    ux_step = 0;
    ux_step_count = 2;
    UX_DISPLAY(ui_address_nanos, ui_address_prepro);   
#endif // #if TARGET_ID
                    
    *flags |= IO_ASYNCH_REPLY;
  }
}

void finalizeParsing(bool direct) {
  uint256_t gasPrice, startGas, uint256;
  uint32_t i;
  uint8_t address[41];
  uint8_t decimals = WEI_TO_ETHER;
  uint8_t *ticker = PIC(chainConfig->coinName);
  uint8_t *feeTicker = PIC(chainConfig->coinName);
  uint8_t tickerOffset = 0;

  // Verify the chain
  if (chainConfig->chainId != 0) {
    uint32_t v = getV(&tmpContent.txContent);
    if (chainConfig->chainId != v) {
        PRINTF("Invalid chainId %d expected %d\n", v, chainConfig->chainId);
        if (direct) {
            THROW(0x6A80);
        }
        else {
            io_seproxyhal_send_status(0x6A80);
            ui_idle();
            return;
        }
    }
  }
  // Store the hash
  cx_hash((cx_hash_t *)&sha3, CX_LAST, tmpCtx.transactionContext.hash, 0, tmpCtx.transactionContext.hash);
    // If there is a token to process, check if it is well known
    if (tokenProvisioned) {
        tokenDefinition_t *currentToken = getKnownToken();
        if (currentToken != NULL) {
            dataPresent = false;
            decimals = currentToken->decimals;
            ticker = currentToken->ticker;
            tmpContent.txContent.destinationLength = 20;
            os_memmove(tmpContent.txContent.destination, dataContext.tokenContext.data + 4 + 12, 20);
            os_memmove(tmpContent.txContent.value.value, dataContext.tokenContext.data + 4 + 32, 32);
            tmpContent.txContent.value.length = 32;
        }
    }  
    else {
      if (dataPresent && !N_storage.dataAllowed) {
          PRINTF("Data field forbidden\n");
          if (direct) {
            THROW(0x6A80);
          }
          else {
            io_seproxyhal_send_status(0x6A80);
            ui_idle();
            return;
          }
      } 
    }
  // Add address
  if (tmpContent.txContent.destinationLength != 0) {
    getEthAddressStringFromBinary(tmpContent.txContent.destination, address, &sha3);
    /*
    addressSummary[0] = '0';
    addressSummary[1] = 'x';
    os_memmove((unsigned char *)(addressSummary + 2), address, 4);
    os_memmove((unsigned char *)(addressSummary + 6), "...", 3);
    os_memmove((unsigned char *)(addressSummary + 9), address + 40 - 4, 4);
    addressSummary[13] = '\0';
    */

    strings.common.fullAddress[0] = '0';
    strings.common.fullAddress[1] = 'x';
    os_memmove((unsigned char *)strings.common.fullAddress+2, address, 40);
    strings.common.fullAddress[42] = '\0';
  }
  else 
  {
    os_memmove((void*)addressSummary, CONTRACT_ADDRESS, sizeof(CONTRACT_ADDRESS));
    strcpy(strings.common.fullAddress, "Contract");
  }
  // Add amount in ethers or tokens
  convertUint256BE(tmpContent.txContent.value.value, tmpContent.txContent.value.length, &uint256);
  tostring256(&uint256, 10, (char *)(G_io_apdu_buffer + 100), 100);
  i = 0;
  while (G_io_apdu_buffer[100 + i]) {
    i++;
  }
  adjustDecimals((char *)(G_io_apdu_buffer + 100), i, (char *)G_io_apdu_buffer, 100, decimals);
  i = 0;
    tickerOffset = 0;
    while (ticker[tickerOffset]) {
        strings.common.fullAmount[tickerOffset] = ticker[tickerOffset];
        tickerOffset++;
    }
    while (G_io_apdu_buffer[i]) {
        strings.common.fullAmount[tickerOffset + i] = G_io_apdu_buffer[i];
        i++;
    }  
  strings.common.fullAmount[tickerOffset + i] = '\0';
  // Compute maximum fee
  convertUint256BE(tmpContent.txContent.gasprice.value, tmpContent.txContent.gasprice.length, &gasPrice);
  convertUint256BE(tmpContent.txContent.startgas.value, tmpContent.txContent.startgas.length, &startGas);
  mul256(&gasPrice, &startGas, &uint256);
  tostring256(&uint256, 10, (char *)(G_io_apdu_buffer + 100), 100);
  i = 0;
  while (G_io_apdu_buffer[100 + i]) {
    i++;
  }
  adjustDecimals((char *)(G_io_apdu_buffer + 100), i, (char *)G_io_apdu_buffer, 100, WEI_TO_ETHER);
  i = 0;
  tickerOffset=0;
  while (feeTicker[tickerOffset]) {
      strings.common.maxFee[tickerOffset] = feeTicker[tickerOffset];
      tickerOffset++;
  }
  tickerOffset++;
  while (G_io_apdu_buffer[i]) {
    strings.common.maxFee[tickerOffset + i] = G_io_apdu_buffer[i];
    i++;
  }
  strings.common.maxFee[tickerOffset + i] = '\0';

#if defined(TARGET_BLUE)
  ui_approval_transaction_blue_init();
#elif defined(TARGET_NANOS)
  ux_step = 0;
  ux_step_count = 5;
  UX_DISPLAY(ui_approval_nanos, ui_approval_prepro);   
#endif // #if TARGET_ID
}

void handleSign(uint8_t p1, uint8_t p2, uint8_t *workBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
  UNUSED(tx);
  parserStatus_e txResult;                    
  uint32_t i;
  if (p1 == P1_FIRST) {
    tmpCtx.transactionContext.pathLength = workBuffer[0];
    if ((tmpCtx.transactionContext.pathLength < 0x01) ||
        (tmpCtx.transactionContext.pathLength > MAX_BIP32_PATH)) {
      PRINTF("Invalid path\n");
      THROW(0x6a80);
    }
    workBuffer++;
    dataLength--;
    for (i = 0; i < tmpCtx.transactionContext.pathLength; i++) {
      tmpCtx.transactionContext.bip32Path[i] =
        (workBuffer[0] << 24) | (workBuffer[1] << 16) |
        (workBuffer[2] << 8) | (workBuffer[3]);
      workBuffer += 4;
      dataLength -= 4;
    }
    dataPresent = false;
    tokenProvisioned = false;
    initTx(&txContext, &sha3, &tmpContent.txContent, customProcessor, NULL);
  } 
  else 
  if (p1 != P1_MORE) {
    THROW(0x6B00);
  }
  if (p2 != 0) {
    THROW(0x6B00);
  }
  if (txContext.currentField == TX_RLP_NONE) {
    PRINTF("Parser not initialized\n");
    THROW(0x6985);
  }
  txResult = processTx(&txContext, workBuffer, dataLength, (chainConfig->kind == CHAIN_KIND_WANCHAIN ? TX_FLAG_TYPE : 0));
  switch (txResult) {
    case USTREAM_SUSPENDED:
      break;
    case USTREAM_FINISHED:
      break;
    case USTREAM_PROCESSING:
      THROW(0x9000);
    case USTREAM_FAULT:
      THROW(0x6A80);
    default:
      PRINTF("Unexpected parser status\n");
      THROW(0x6A80);
  }

  *flags |= IO_ASYNCH_REPLY;

  if (txResult == USTREAM_FINISHED) {
    finalizeParsing(true);
  }  
}

void handleGetAppConfiguration(uint8_t p1, uint8_t p2, uint8_t *workBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
  UNUSED(p1);
  UNUSED(p2);
  UNUSED(workBuffer);
  UNUSED(dataLength);
  UNUSED(flags);
  G_io_apdu_buffer[0] = (N_storage.dataAllowed ? 0x01 : 0x00);
  G_io_apdu_buffer[1] = LEDGER_MAJOR_VERSION;
  G_io_apdu_buffer[2] = LEDGER_MINOR_VERSION;
  G_io_apdu_buffer[3] = LEDGER_PATCH_VERSION;
  *tx = 4;
  THROW(0x9000);
}

void handleSignPersonalMessage(uint8_t p1, uint8_t p2, uint8_t *workBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
  UNUSED(tx);
  uint8_t hashMessage[32];
  if (p1 == P1_FIRST) {
    char tmp[11];
    uint32_t index;
    uint32_t base = 10;
    uint8_t pos = 0;
    uint32_t i;
    tmpCtx.messageSigningContext.pathLength = workBuffer[0];
    if ((tmpCtx.messageSigningContext.pathLength < 0x01) ||
        (tmpCtx.messageSigningContext.pathLength > MAX_BIP32_PATH)) {
        PRINTF("Invalid path\n");
        THROW(0x6a80);
    }
    workBuffer++;
    dataLength--;
    for (i = 0; i < tmpCtx.messageSigningContext.pathLength; i++) {
        tmpCtx.messageSigningContext.bip32Path[i] =
          (workBuffer[0] << 24) | (workBuffer[1] << 16) |
          (workBuffer[2] << 8) | (workBuffer[3]);
        workBuffer += 4;
        dataLength -= 4;
    }
    tmpCtx.messageSigningContext.remainingLength =
      (workBuffer[0] << 24) | (workBuffer[1] << 16) |
      (workBuffer[2] << 8) | (workBuffer[3]);
    workBuffer += 4;
    dataLength -= 4;
    // Initialize message header + length
    cx_keccak_init(&sha3, 256);
    cx_hash((cx_hash_t *)&sha3, 0, SIGN_MAGIC, sizeof(SIGN_MAGIC) - 1, NULL);
    for (index = 1; (((index * base) <= tmpCtx.messageSigningContext.remainingLength) &&
                         (((index * base) / base) == index));
             index *= base);
    for (; index; index /= base) {
      tmp[pos++] = '0' + ((tmpCtx.messageSigningContext.remainingLength / index) % base);
    }
    tmp[pos] = '\0';
    cx_hash((cx_hash_t *)&sha3, 0, tmp, pos, NULL);
    cx_sha256_init(&tmpContent.sha2);
  } 
  else if (p1 != P1_MORE) {
    THROW(0x6B00);
  }
  if (p2 != 0) {
    THROW(0x6B00);
  }
  if (dataLength > tmpCtx.messageSigningContext.remainingLength) {
      THROW(0x6A80);
  }
  cx_hash((cx_hash_t *)&sha3, 0, workBuffer, dataLength, NULL);
  cx_hash((cx_hash_t *)&tmpContent.sha2, 0, workBuffer, dataLength, NULL);
  tmpCtx.messageSigningContext.remainingLength -= dataLength;
  if (tmpCtx.messageSigningContext.remainingLength == 0) {
    cx_hash((cx_hash_t *)&sha3, CX_LAST, workBuffer, 0, tmpCtx.messageSigningContext.hash);
    cx_hash((cx_hash_t *)&tmpContent.sha2, CX_LAST, workBuffer, 0, hashMessage);

#define HASH_LENGTH 4
    array_hexstr(strings.common.fullAddress, hashMessage, HASH_LENGTH / 2);
    strings.common.fullAddress[HASH_LENGTH / 2 * 2] = '.';
    strings.common.fullAddress[HASH_LENGTH / 2 * 2 + 1] = '.';
    strings.common.fullAddress[HASH_LENGTH / 2 * 2 + 2] = '.';
    array_hexstr(strings.common.fullAddress + HASH_LENGTH / 2 * 2 + 3, hashMessage + 32 - HASH_LENGTH / 2, HASH_LENGTH / 2);

#if defined(TARGET_BLUE)
    ui_approval_message_sign_blue_init();
#elif defined(TARGET_NANOS)
    ux_step = 0;
    ux_step_count = 2;
    UX_DISPLAY(ui_approval_signMessage_nanos,
                   ui_approval_signMessage_prepro);
#endif // #if TARGET_ID

    *flags |= IO_ASYNCH_REPLY;

  } else {
    THROW(0x9000);
  }
}

void handleApdu(volatile unsigned int *flags, volatile unsigned int *tx) {
  unsigned short sw = 0;

  BEGIN_TRY {
    TRY {
      if (G_io_apdu_buffer[OFFSET_CLA] != CLA) {
        THROW(0x6E00);
      }

      switch (G_io_apdu_buffer[OFFSET_INS]) {
        case INS_GET_PUBLIC_KEY: 
          handleGetPublicKey(G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2], G_io_apdu_buffer + OFFSET_CDATA, G_io_apdu_buffer[OFFSET_LC], flags, tx);
          break;

        case INS_SIGN: 
          handleSign(G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2], G_io_apdu_buffer + OFFSET_CDATA, G_io_apdu_buffer[OFFSET_LC], flags, tx);
          break;

        case INS_GET_APP_CONFIGURATION: 
          handleGetAppConfiguration(G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2], G_io_apdu_buffer + OFFSET_CDATA, G_io_apdu_buffer[OFFSET_LC], flags, tx);
          break;

        case INS_SIGN_PERSONAL_MESSAGE: 
          handleSignPersonalMessage(G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2], G_io_apdu_buffer + OFFSET_CDATA, G_io_apdu_buffer[OFFSET_LC], flags, tx);
          break;

#if 0
        case 0xFF: // return to dashboard
          goto return_to_dashboard;
#endif          

        default:
          THROW(0x6D00);
          break;
      }
    }
    CATCH(EXCEPTION_IO_RESET) {
      THROW(EXCEPTION_IO_RESET);
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
        G_io_apdu_buffer[*tx] = sw >> 8;
        G_io_apdu_buffer[*tx + 1] = sw;
        *tx += 2;
      }
      FINALLY {
      }
  }
  END_TRY;
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

                handleApdu(&flags, &tx);
            }
            CATCH(EXCEPTION_IO_RESET) {
              THROW(EXCEPTION_IO_RESET);
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
                if (e != 0x9000) {
                    flags &= ~IO_ASYNCH_REPLY;
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

//return_to_dashboard:
    return;
}

// override point, but nothing more to do
void io_seproxyhal_display(const bagl_element_t *element) {
  io_seproxyhal_display_default((bagl_element_t *)element);
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

    case SEPROXYHAL_TAG_STATUS_EVENT:
        if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID && !(U4BE(G_io_seproxyhal_spi_buffer, 3) & SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
         THROW(EXCEPTION_IO_RESET);
        }
        // no break is intentional
    default:
        UX_DEFAULT_EVENT();
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        UX_DISPLAYED_EVENT({});
        break;

    case SEPROXYHAL_TAG_TICKER_EVENT:
        UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, 
        {
          if (UX_ALLOWED) {
            if (ux_step_count) {
              // prepare next screen
              ux_step = (ux_step+1)%ux_step_count;
              // redisplay screen
              UX_REDISPLAY(); 
            }
          }
        });
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

chain_config_t const C_chain_config = {
  .coinName = CHAINID_COINNAME " ",
  .chainId = CHAIN_ID,
  .kind = CHAIN_KIND,
#ifdef TARGET_BLUE
  .color_header = COLOR_APP,
  .color_dashboard = COLOR_APP_LIGHT,
  .header_text = CHAINID_UPCASE,
#endif // TARGET_BLUE
};

__attribute__((section(".boot"))) int main(int arg0) {
#ifdef USE_LIB_ETHEREUM
    chain_config_t local_chainConfig;
    os_memmove(&local_chainConfig, &C_chain_config, sizeof(chain_config_t));
    unsigned int libcall_params[3];
    unsigned char coinName[sizeof(CHAINID_COINNAME)];
    strcpy(coinName, CHAINID_COINNAME);
#ifdef TARGET_BLUE
    unsigned char coinNameUP[sizeof(CHAINID_UPCASE)];
    strcpy(coinNameUP, CHAINID_UPCASE);
    local_chainConfig.header_text = coinNameUP;
#endif // TARGET_BLUE
    local_chainConfig.coinName = coinName;
    BEGIN_TRY {
        TRY {
            // ensure syscall will accept us
            check_api_level(CX_COMPAT_APILEVEL);
            // delegate to Ethereum app/lib
            libcall_params[0] = "Ethereum";
            libcall_params[1] = 0x100; // use the Init call, as we won't exit
            libcall_params[2] = &local_chainConfig;
            os_lib_call(&libcall_params);
        }
        FINALLY {
            app_exit();
        }
    }
    END_TRY;    
#else    
    // exit critical section
    __asm volatile("cpsie i");

    if (arg0) {
        if (((unsigned int *)arg0)[0] != 0x100) {
            os_lib_throw(INVALID_PARAMETER);
        }      
        chainConfig = (chain_config_t *)((unsigned int *)arg0)[1];
    }
    else {
        chainConfig = (chain_config_t *)PIC(&C_chain_config);
    }

    os_memset(&txContext, 0, sizeof(txContext));

    // ensure exception will work as planned
    os_boot();

    for (;;) {
        UX_INIT();

        BEGIN_TRY {
            TRY {
                io_seproxyhal_init();

                if (N_storage.initialized != 0x01) {
                  internalStorage_t storage;
                  storage.dataAllowed = 0x00;
                  storage.contractDetails = 0x00;
                  storage.initialized = 0x01;
                  nvm_write(&N_storage, (void*)&storage, sizeof(internalStorage_t));
                }
                dataAllowed = N_storage.dataAllowed;
                contractDetails = N_storage.contractDetails;

                USB_power(0);
                USB_power(1);

    			      ui_idle();

    #if defined(TARGET_BLUE)
                // setup the status bar colors (remembered after wards, even more if another app does not resetup after app switch)
                UX_SET_STATUS_BAR_COLOR(0xFFFFFF, chainConfig->color_header);
    #endif // #if defined(TARGET_BLUE)
    			
                sample_main();
            }
            CATCH(EXCEPTION_IO_RESET) {
              // reset IO and UX before continuing
              continue;
            }
            CATCH_ALL {
              break;
            }
            FINALLY {
            }
        }
        END_TRY;
    }
	  app_exit();
#endif
    return 0;
}
