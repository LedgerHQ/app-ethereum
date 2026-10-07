#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include "unity.h"

// Keccak init return code — declared here, defined in mocks/sdk_stubs.c.
extern uint32_t g_keccak_init_ret;

// app_exit / app_quit / send_swap_error_simple are noreturn. Their weak
// defaults in system_stubs.c either while(1) (process hangs) or longjmp
// to g_noreturn_jmp if g_noreturn_armed is true. Tests that exercise a
// code path that *should* call one of them arm the jump, run the statement
// inside a setjmp, and check g_noreturn_calls. EXPECT_NORETURN() packages
// the boilerplate.
extern jmp_buf g_noreturn_jmp;
extern bool g_noreturn_armed;
extern int g_noreturn_calls;

#define EXPECT_NORETURN(stmt)                                     \
    do {                                                          \
        g_noreturn_armed = true;                                  \
        g_noreturn_calls = 0;                                     \
        if (setjmp(g_noreturn_jmp) == 0) {                        \
            (stmt);                                               \
            g_noreturn_armed = false;                             \
            TEST_FAIL_MESSAGE("expected noreturn was not taken"); \
        }                                                         \
        g_noreturn_armed = false;                                 \
    } while (0)

// The chain config defined in mocks/app_globals.c. A couple of tests
// (test_cmd_get_public_key, test_eth_swap_utils) mutate chain_id to
// drive multi-chain assertions; expose it through wraps.h so they
// don't have to add their own extern decl.
struct chain_config_s;
extern struct chain_config_s g_chainConfig;

// NVRAM-storage backing store for the production N_storage_real alias.
// test_commands_7702 toggles eip7702_enable to gate the auth flow.
struct internalStorage_t;
extern struct internalStorage_t g_n_storage_writable;

// NBGL warning state. The real def lives in src/nbgl/ui_home.c which is
// not linked in host tests; mocks/app_globals.c provides a zero-init
// fallback. Tests need the full type to poke its fields, so the extern
// decl is in ui_nbgl.h (production) -- include that directly from the
// test files that read warning.predefinedSet / warning.prelude.

// Backing storage for txContext.content. Three tests (test_network,
// test_param_enum, test_provide_map_entry) point txContext.content at
// this and write fields directly. Include shared_context.h (which
// pulls eth_plugin_interface.h -> tx_content.h) before referencing.
struct txContent_t;
extern struct txContent_t g_tx_content;
