#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "eip712_v1_ui_logic.h"
#include "shared_context.h"
#include "ux.h"

void ui_idle(void);
void ui_error_blind_signing(void);

/**
 * Keep the error screen the current command has just displayed.
 *
 * Without this the main loop returns to the home screen when it answers the host.
 * The request applies to that one reply and is cleared once read.
 */
void ui_hold_error_screen(void);

/**
 * Whether the current command asked to keep its error screen, clearing the request.
 */
bool ui_error_screen_held(void);
void ui_display_public_eth2(void);
void ui_display_privacy_public_key(void);
void ui_display_privacy_shared_secret(void);
void ui_display_public_key(const uint64_t *chain_id);
void ui_confirm_selector(void);
void ui_confirm_parameter(void);
void ui_display_safe_account(void);

// EIP-191
void ui_191_start(const char *message);

/**
 * Whether an EIP-191 message can be fully reached in the review's details view.
 *
 * @param[in] message the NUL-terminated display string
 * @return false if rendering it would need more pages than the UI can address
 */
bool ui_191_message_fits(const char *message);

// EIP-712
bool ui_sign_712_v1(e_eip712_filtering_mode filtering_mode);
bool ui_sign_712_v0(void);

// Generic clear-signing
bool ui_gcs(void);
void ui_gcs_cleanup(void);

// EIP-7702
bool ui_sign_7702_auth(void);
bool ui_sign_7702_revocation(void);
void ui_error_no_7702(void);
void ui_error_no_7702_whitelist(void);

// Swap
void ui_swap_show_signing(void);

// UI callbacks
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
unsigned int auth_7702_ok_cb(void);
unsigned int auth_7702_cancel_cb(void);

uint16_t io_seproxyhal_send_status(uint16_t sw, uint32_t tx, bool reset, bool idle);
