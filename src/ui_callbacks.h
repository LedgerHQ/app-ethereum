#ifndef _UI_CALLBACKS_H_
#define _UI_CALLBACKS_H_

#include "shared_context.h"
#include "ux.h"

#ifdef HAVE_NBGL
typedef int bagl_element_t;
#endif

unsigned int io_seproxyhal_touch_settings(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_exit(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_tx_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_tx_cancel(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_address_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_address_cancel(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_signMessage_ok(void);
unsigned int io_seproxyhal_touch_signMessage_cancel(void);
unsigned int io_seproxyhal_touch_data_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_data_cancel(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_eth2_address_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_privacy_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_privacy_cancel(const bagl_element_t *e);

void ui_warning_contract_data(void);

void io_seproxyhal_send_status(uint32_t sw);
void finalizeParsing(bool direct);
extraInfo_t *getKnownToken(uint8_t *contractAddress);

#endif  // _UI_CALLBACKS_H_
