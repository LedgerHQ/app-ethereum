#pragma once

#include "shared_context.h"
#include "ux.h"

#ifdef HAVE_NBGL
typedef int bagl_element_t;
#endif

unsigned int io_seproxyhal_touch_tx_ok(void);
unsigned int io_seproxyhal_touch_tx_cancel(void);
unsigned int io_seproxyhal_touch_address_ok(void);
unsigned int io_seproxyhal_touch_address_cancel(void);
unsigned int io_seproxyhal_touch_signMessage_ok(void);
unsigned int io_seproxyhal_touch_signMessage_cancel(void);
unsigned int io_seproxyhal_touch_data_ok(void);
unsigned int io_seproxyhal_touch_data_cancel(void);
unsigned int io_seproxyhal_touch_eth2_address_ok(void);
unsigned int io_seproxyhal_touch_privacy_ok(void);
unsigned int io_seproxyhal_touch_privacy_cancel(void);

void ui_warning_contract_data(void);

uint32_t io_seproxyhal_send_status(uint32_t sw, uint32_t tx, bool reset, bool idle);
