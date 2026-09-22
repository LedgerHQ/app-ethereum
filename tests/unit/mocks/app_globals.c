/**
 * @file app_globals.c
 * @brief Shared definitions for app-side global variables.
 *
 * The Ethereum app declares its top-level state (txContext, tmpContent,
 * strings, the chain config pointer, the swap context, NVRAM storage,
 * etc.) as file-scope globals in src/main.c. Unit tests don't link
 * main.c, so historically each test_*.c carried its own copy of these
 * definitions (~6-15 lines of boilerplate per file). Centralising them
 * here means a test only needs to `#include "shared_context.h"` (which
 * it already does) and reference the names directly.
 *
 * Every definition is __attribute__((weak)) so a test target that
 * links the real production source file (e.g. test_network links
 * src/network.c which defines g_unknown_ticker) keeps its strong
 * definition without a multiple-definition link error.
 *
 * This file is NOT linked into the lightweight test targets (test_path_*,
 * test_calldata, test_token_info, test_tlv_apdu) because they don't
 * compile shared_context.h (no HAVE_SHA256 / HAVE_HASH defines). Tests
 * that need to mutate g_chainConfig do so through the extern decl in
 * mocks/wraps.h.
 */

#include <stdbool.h>
#include <stdint.h>

#include "shared_context.h"
#include "network.h"
#include "tx_ctx.h"         // s_calldata + extern g_parked_calldata
#include "nbgl_use_case.h"  // nbgl_warning_t

// Forward-declare to avoid pulling network_info.h, which lives in a
// per-target include path (provide_network_info) and isn't visible
// from every test target's compile line.
typedef struct network_info_s network_info_t;

// =============================================================================
// app/src/main.c -- shared transaction / UI context
// =============================================================================

WEAK strings_t strings;
WEAK txContext_t txContext;
WEAK tmpContent_t tmpContent;
WEAK tmpCtx_t tmpCtx;
WEAK dataContext_t dataContext;
WEAK uint8_t appState = APP_STATE_IDLE;
WEAK uint8_t G_io_tx_buffer[OS_IO_BUFFER_SIZE + 1];
WEAK pluginType_t pluginType;
WEAK uint32_t eth2WithdrawalIndex;
WEAK const caller_app_t *g_caller_app = NULL;

// =============================================================================
// app/src/main.c -- chain config (ETH mainnet by default)
// =============================================================================
// Tests that need a different chain mutate g_chainConfig.chain_id
// directly (extern in mocks/wraps.h).

WEAK chain_config_t g_chainConfig = {.ticker = "ETH", .chain_id = 1, .coin_type = 60};
WEAK const chain_config_t *g_chain_config = &g_chainConfig;

// =============================================================================
// app/src/main.c -- NVRAM storage scaffolding
// =============================================================================
// The real N_storage_real is a const-qualified alias into NVRAM (rewritten
// through nvm_write syscalls). In host tests we point it at a writable
// backing store so anything that derefs it works without faulting.

WEAK internalStorage_t g_n_storage_writable;
extern const internalStorage_t N_storage_real WEAK __attribute__((alias("g_n_storage_writable")));

// =============================================================================
// SDK lib_standard_app/swap_utils.h + app/src/swap/eth_swap_utils.h
// =============================================================================
// Swap-mode signaling globals. Tests don't exercise the actual swap-app
// handshake so the defaults (all false / SWAP_MODE_STANDARD) are fine.
// G_swap_checked has a real def in src/swap/eth_swap_utils.c and the
// G_swap_expected_value* pair in src/swap/handle_swap_sign_transaction.c,
// so the weak qualifier defers to them when those sources are linked.

WEAK volatile bool G_called_from_swap;
WEAK volatile bool G_swap_response_ready;
WEAK bool G_swap_checked;
WEAK swap_mode_t G_swap_mode;
WEAK uint64_t G_swap_expected_chain_id;
WEAK bool G_swap_tx_had_calldata;
WEAK bool G_swap_calldata_validated;
WEAK uint8_t G_swap_expected_token_address[ADDRESS_LENGTH];
WEAK bool G_swap_has_expected_token_address;
WEAK swap_value_check_t G_swap_expected_value_check;
WEAK uint8_t G_swap_expected_value[INT256_LENGTH];

// =============================================================================
// app/src/features/generic_tx_parser/tx_ctx.c
// =============================================================================
// g_parked_calldata is the buffered calldata captured during the
// EIP-712 preview before the user confirms.

WEAK s_calldata *g_parked_calldata = NULL;

// =============================================================================
// app/src/features/provide_network_info/network_info.c
// =============================================================================
// Head of the host-provided dynamic network list, plus the per-APDU
// landing pointer the descriptor parser sets so downstream commands
// (e.g. cmd_network_icon) can attach payload to the partially-built
// entry. test_network_info links the real network_info.c so the
// strong defs there win; the weak fallbacks here cover the other
// tests that pull the headers without linking the source.

WEAK network_info_t *g_dynamic_network_list = NULL;
WEAK network_info_t *g_last_added_network = NULL;

// =============================================================================
// app/src/features/sign_tx/cmd_sign_tx.c
// =============================================================================
// Keccak context handle used by the multi-frame TX hash path. The real
// def lives in cmd_sign_tx.c, so test_cmd_sign_tx (which links it)
// keeps the strong def; test_logic_sign_tx_fee falls back to the weak
// here.

WEAK cx_sha3_t *g_tx_hash_ctx = NULL;

// =============================================================================
// app/src/nbgl/ui_home.c
// =============================================================================
// NBGL warning structure read by the UI screens. The real def lives in
// ui_home.c which we don't link in host tests; provide a zero-inited
// fallback so the few tests that pull set_gating_ui_screen /
// set_tx_simulation_ui_screen link.

WEAK nbgl_warning_t warning;

// =============================================================================
// Test-side fixture: backing storage for txContext.content
// =============================================================================
// Three tests (test_network, test_param_enum, test_provide_map_entry) point
// txContext.content at this and write fields directly. Each test process
// runs independently so there is no cross-test bleed at runtime.
// g_fake_tx_info (test_param_*) is intentionally left as 5 file-local
// instances: s_tx_info is an anonymous-struct typedef so an extern decl
// in wraps.h would have to pull gtp_tx_info.h's cx.h chain through every
// test target, and the savings (one line per test) don't justify it.

WEAK txContent_t g_tx_content;
