#include "shared_context.h"
#include "ux.h"

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
unsigned int io_seproxyhal_touch_signMessage712_v0_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_signMessage712_v0_cancel(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_eth2_address_ok(const bagl_element_t *e);

void ui_idle(void);

void io_seproxyhal_send_status(uint32_t sw);
void format_signature_out(const uint8_t *signature);
void finalizeParsing(bool direct);
tokenDefinition_t *getKnownToken(uint8_t *contractAddress);
