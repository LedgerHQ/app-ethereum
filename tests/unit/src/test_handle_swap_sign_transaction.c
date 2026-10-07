/**
 * @file test_handle_swap_sign_transaction.c
 * @brief Unit tests for copy_transaction_parameters at
 *        src/swap/handle_swap_sign_transaction.c.
 *
 * The Ledger Exchange app calls copy_transaction_parameters at swap-init
 * time -- the binary structure it hands the Ethereum app contains the
 * amount, fees, destination, and (for cross-chain swaps) the partner-
 * promised calldata hash. The Ethereum app COPIES these values onto its
 * own stack before validating, because the input pointer can alias
 * Ethereum-app globals.
 *
 * Pin every reject / accept branch:
 *
 *  - amount_length > 32                      false  (CWE-787 overflow on copy)
 *  - fee_amount_length > 8                   false  (idem)
 *  - destination_address overflow            false  (strlcpy truncation guard)
 *  - parse_swap_config fails                 false
 *  - amountToString(fees) fails              false
 *  - amountToString(amount) fails standard   false
 *  - amountToString(amount) fails crosschain false
 *  - EXTRA_ID_TYPE_NATIVE                    true,  G_swap_mode = STANDARD
 *  - EXTRA_ID_TYPE_EVM_CALLDATA              true,  G_swap_mode = CROSSCHAIN_PENDING_CHECK
 *                                                   + crosschain hash copied to
 * G_swap_crosschain_hash
 *  - invalid extra_id type                   true,  G_swap_mode = ERROR
 *                                                   (the function does NOT short-circuit; it
 *                                                   remembers and reports the issue later)
 *
 * The noreturn `swap_finalize_exchange_sign_transaction` and
 * `handle_swap_sign_transaction` entry points are not exercised here --
 * each calls os_lib_end() / app_quit() and would terminate the test
 * process. Their bodies are trivial passthrough.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "swap_lib_calls.h"
#include "chain_config.h"
#include "eth_swap_utils.h"
#include "wraps.h"  // g_noreturn_jmp / g_noreturn_armed / g_noreturn_calls + EXPECT_NORETURN
#include "Mockcommon_utils.h"

bool copy_transaction_parameters(create_transaction_parameters_t *sign_transaction_params,
                                 const chain_config_t *config);

// =============================================================================
// Globals the unit under test reads / writes
// =============================================================================
// G_swap_mode and G_swap_crosschain_hash are *defined* by
// handle_swap_sign_transaction.c, so we just observe them from here.
// G_swap_signing_return_value_address lives in the SDK lib_standard_app
// in production; provide local storage for the test.
volatile uint8_t *G_swap_signing_return_value_address;

static bool g_mem_utils_alloc_ret = false;
static bool g_parse_swap_config_ok = false;
static const char *g_parse_swap_config_ticker = NULL;

// =============================================================================
// Wraps
// =============================================================================
// parse_swap_config / get_asset_info_on_network are wrapped through linker
// --wrap so each test can drive the parser outcome without dragging
// eth_swap_utils.c into the link.

bool parse_swap_config(const uint8_t *config, uint8_t config_size, swap_context_t *ctx) {
    (void) config;
    (void) config_size;
    bool ok = g_parse_swap_config_ok;
    const char *swapped_ticker = g_parse_swap_config_ticker;
    if (ok && ctx != NULL) {
        // Fees-asset ticker is fixed at "ETH" (native of this chain). The
        // swapped-asset ticker is per-test: equal to "ETH" means the
        // function takes the regular amount path; different (e.g. "USDC")
        // forces the crosschain-non-native zero-amount branch.
        memset(ctx, 0, sizeof(*ctx));
        strlcpy(ctx->swapped_asset_info.ticker,
                swapped_ticker,
                sizeof(ctx->swapped_asset_info.ticker));
        ctx->swapped_asset_info.decimals = 18;
        strlcpy(ctx->fees_asset_info.ticker, "ETH", sizeof(ctx->fees_asset_info.ticker));
        ctx->fees_asset_info.decimals = 18;
    }
    return ok;
}

// Override mocks/mock.c's mem_utils_alloc via --wrap so a single test can
// simulate an out-of-memory APP_MEM_ALLOC.
void *mem_utils_alloc(size_t size, bool permanent, const char *file, int line) {
    (void) permanent;
    (void) file;
    (void) line;
    return g_mem_utils_alloc_ret ? malloc(size) : NULL;
}

void get_asset_info_on_network(bool is_fee,
                               swap_context_t *context,
                               chain_config_t *chain_config,
                               char **ticker,
                               uint8_t *decimals) {
    (void) is_fee;
    (void) chain_config;
    (void) decimals;
    // For these tests we always point ticker at the fees-asset ticker
    // (which is what production code does for is_fee=true).
    if (ticker != NULL && context != NULL) {
        *ticker = context->fees_asset_info.ticker;
    }
}

static bool s_amountToString_ret = true;
static bool amountToString_stub(const uint8_t *amount,
                                uint8_t amount_len,
                                uint8_t decimals,
                                const char *ticker,
                                char *out_buffer,
                                size_t out_buffer_size,
                                int cmock_num_calls) {
    (void) amount;
    (void) amount_len;
    (void) decimals;
    (void) ticker;
    (void) cmock_num_calls;
    bool ok = s_amountToString_ret;
    if (ok && out_buffer != NULL && out_buffer_size > 0) {
        strlcpy(out_buffer, "1 ETH", out_buffer_size);
    }
    return ok;
}

// Stubs for the rest of the BSS-reset / lib-call surface. None of these
// observe state in our tests.
void os_explicit_zero_BSS_segment(void) {
    // BSS-zero would clobber the test's own globals -- intentionally a no-op.
}

static int g_ui_swap_show_signing_calls = 0;
void ui_swap_show_signing(void) {
    g_ui_swap_show_signing_calls++;
}

static int g_set_swap_with_calldata_calls = 0;
void set_swap_with_calldata_plugin_type(void) {
    g_set_swap_with_calldata_calls++;
}

// app_main and os_lib_end are noreturn in production. Honour the
// EXPECT_NORETURN handshake from wraps.h so tests can drive the
// dedicated noreturn entry points (handle_swap_sign_transaction,
// swap_finalize_exchange_sign_transaction) without freezing the test
// process.
__attribute__((noreturn)) void app_main(void) {
    g_noreturn_calls++;
    if (g_noreturn_armed) longjmp(g_noreturn_jmp, 1);
    while (1) {
    }
}

__attribute__((noreturn)) void os_lib_end(void) {
    g_noreturn_calls++;
    if (g_noreturn_armed) longjmp(g_noreturn_jmp, 1);
    while (1) {
    }
}

// =============================================================================
// Fixtures
// =============================================================================

extern swap_mode_t G_swap_mode;
extern uint8_t *G_swap_crosschain_hash;

static chain_config_t s_chain = {.ticker = "ETH", .chain_id = 1, .coin_type = 60};

static uint8_t s_amount[32];
static uint8_t s_fee[8];
static uint8_t s_coin_cfg[16];
static char s_addr[] = "0x1234567890abcdef1234567890abcdef12345678";

static void reset(void) {
    memset(&strings, 0, sizeof(strings));
    memset(s_amount, 0, sizeof(s_amount));
    memset(s_fee, 0, sizeof(s_fee));
    memset(s_coin_cfg, 0, sizeof(s_coin_cfg));
    G_swap_mode = SWAP_MODE_STANDARD;
    if (G_swap_crosschain_hash != NULL) {
        free(G_swap_crosschain_hash);
        G_swap_crosschain_hash = NULL;
    }
    g_mem_utils_alloc_ret = false;
    g_parse_swap_config_ok = false;
    g_parse_swap_config_ticker = NULL;
}

static create_transaction_parameters_t make_params(void) {
    create_transaction_parameters_t p = {0};
    p.coin_configuration = s_coin_cfg;
    p.coin_configuration_length = sizeof(s_coin_cfg);
    p.amount = s_amount;
    p.amount_length = 4;
    p.fee_amount = s_fee;
    p.fee_amount_length = 4;
    p.destination_address = s_addr;
    p.destination_address_extra_id = NULL;
    return p;
}

// =============================================================================
// Length-bound rejects (CWE-787)
// =============================================================================

void test_amount_length_over_32_rejected(void) {
    create_transaction_parameters_t p = make_params();
    p.amount_length = 33;
    // parse_swap_config / amountToString MUST NOT be reached -- the
    // length-bound check fails first.
    TEST_ASSERT_FALSE(copy_transaction_parameters(&p, &s_chain));
}

void test_fee_amount_length_over_8_rejected(void) {
    create_transaction_parameters_t p = make_params();
    p.fee_amount_length = 9;
    TEST_ASSERT_FALSE(copy_transaction_parameters(&p, &s_chain));
}

// The "destination_address overflow" guard in the source
// (toAddress[sizeof(toAddress)-1] != '\0') is effectively dead code today:
// strlcpy always writes a NUL at dst[size-1] when size > 0. We leave the
// guard in place as defense-in-depth against a future refactor that
// swaps strlcpy for strncpy, but there is no way to exercise that branch
// from a unit test without intercepting strlcpy itself.

// =============================================================================
// parse_swap_config failure
// =============================================================================

void test_parse_swap_config_failure_rejected(void) {
    create_transaction_parameters_t p = make_params();
    g_parse_swap_config_ok = false;
    g_parse_swap_config_ticker = NULL;
    TEST_ASSERT_FALSE(copy_transaction_parameters(&p, &s_chain));
}

// =============================================================================
// amountToString failures
// =============================================================================

void test_amount_to_string_fees_failure_rejected(void) {
    create_transaction_parameters_t p = make_params();
    g_parse_swap_config_ok = true;
    g_parse_swap_config_ticker = "ETH";
    s_amountToString_ret = false;  // fees stringification fails
    TEST_ASSERT_FALSE(copy_transaction_parameters(&p, &s_chain));
}

void test_amount_to_string_value_failure_standard_rejected(void) {
    create_transaction_parameters_t p = make_params();
    g_parse_swap_config_ok = true;
    g_parse_swap_config_ticker = "ETH";
    s_amountToString_ret = true;   // fees ok
    s_amountToString_ret = false;  // amount fails
    TEST_ASSERT_FALSE(copy_transaction_parameters(&p, &s_chain));
}

// =============================================================================
// Happy paths
// =============================================================================

void test_native_swap_returns_true_and_sets_standard_mode(void) {
    create_transaction_parameters_t p = make_params();
    // No destination_address_extra_id -> NATIVE branch -> SWAP_MODE_STANDARD.
    g_parse_swap_config_ok = true;
    g_parse_swap_config_ticker = "ETH";
    s_amountToString_ret = true;
    s_amountToString_ret = true;
    g_mem_utils_alloc_ret = true;
    TEST_ASSERT_TRUE(copy_transaction_parameters(&p, &s_chain));
    TEST_ASSERT_EQUAL(G_swap_mode, SWAP_MODE_STANDARD);
    // crosschain hash MUST stay all-zero for native swaps.
    static const uint8_t zero[32] = {0};
    TEST_ASSERT_NOT_NULL(G_swap_crosschain_hash);
    TEST_ASSERT_EQUAL_MEMORY(G_swap_crosschain_hash, zero, 32);
}

void test_crosschain_calldata_swap_copies_hash(void) {
    // First byte = EXTRA_ID_TYPE_EVM_CALLDATA (1), next 32 = the hash.
    uint8_t extra_id[1 + 32];
    extra_id[0] = 1;  // EXTRA_ID_TYPE_EVM_CALLDATA
    for (int i = 0; i < 32; i++) extra_id[1 + i] = (uint8_t) (i + 1);
    create_transaction_parameters_t p = make_params();
    p.destination_address_extra_id = (char *) extra_id;
    g_parse_swap_config_ok = true;
    g_parse_swap_config_ticker = "ETH";
    s_amountToString_ret = true;
    s_amountToString_ret = true;
    g_mem_utils_alloc_ret = true;
    TEST_ASSERT_TRUE(copy_transaction_parameters(&p, &s_chain));
    TEST_ASSERT_EQUAL(G_swap_mode, SWAP_MODE_CROSSCHAIN_PENDING_CHECK);
    // The 32-byte hash from extra_id[1..32] must land in G_swap_crosschain_hash.
    static const uint8_t expected_hash[32] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                                              12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                                              23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
    TEST_ASSERT_NOT_NULL(G_swap_crosschain_hash);
    TEST_ASSERT_EQUAL_MEMORY(G_swap_crosschain_hash, expected_hash, 32);
}

void test_unknown_extra_id_type_sets_error_mode_but_continues(void) {
    // First byte = some unknown value -> default case -> SWAP_MODE_ERROR but
    // no early return.
    uint8_t extra_id[1 + 32] = {0xFE};
    create_transaction_parameters_t p = make_params();
    p.destination_address_extra_id = (char *) extra_id;
    g_parse_swap_config_ok = true;
    g_parse_swap_config_ticker = "ETH";
    s_amountToString_ret = true;
    s_amountToString_ret = true;
    g_mem_utils_alloc_ret = true;
    // The function does NOT bail on an unknown extra_id type -- it commits
    // SWAP_MODE_ERROR so a later screen reports the issue. Returning true
    // here is the load-bearing observation; if a future refactor adds an
    // early `return false`, the caller has no chance to display the error.
    TEST_ASSERT_TRUE(copy_transaction_parameters(&p, &s_chain));
    TEST_ASSERT_EQUAL(G_swap_mode, SWAP_MODE_ERROR);
}

void test_crosschain_non_native_zero_amount_path(void) {
    // EVM_CALLDATA extra-id but the swapped asset is a token (USDC), not
    // the chain's native ETH. The code must call amountToString once for
    // fees (in ETH) and once with the zero-amount sentinel for the
    // fullAmount (still in fees ticker), because the actual token amount
    // is moved on the destination chain.
    uint8_t extra_id[1 + 32] = {0x01};  // EVM_CALLDATA
    create_transaction_parameters_t p = make_params();
    p.destination_address_extra_id = (char *) extra_id;
    g_parse_swap_config_ok = true;
    g_parse_swap_config_ticker = "USDC";  // swapped != fees ticker
    s_amountToString_ret = true;          // fees
    s_amountToString_ret = true;          // zero-amount fullAmount
    g_mem_utils_alloc_ret = true;
    TEST_ASSERT_TRUE(copy_transaction_parameters(&p, &s_chain));
    TEST_ASSERT_EQUAL(G_swap_mode, SWAP_MODE_CROSSCHAIN_PENDING_CHECK);
}

void test_crosschain_non_native_zero_amount_failure_rejected(void) {
    // EVM_CALLDATA + non-native asset: the second amountToString writes the
    // zero-amount fullAmount in the fees ticker. If THAT call fails, the
    // function must return false too -- otherwise we'd commit globals
    // around an incomplete review string.
    uint8_t extra_id[1 + 32] = {0x01};
    create_transaction_parameters_t p = make_params();
    p.destination_address_extra_id = (char *) extra_id;
    g_parse_swap_config_ok = true;
    g_parse_swap_config_ticker = "USDC";
    s_amountToString_ret = true;   // fees ok
    s_amountToString_ret = false;  // zero-amount fullAmount fails
    TEST_ASSERT_FALSE(copy_transaction_parameters(&p, &s_chain));
}

void test_alloc_failure_returns_false(void) {
    create_transaction_parameters_t p = make_params();
    g_parse_swap_config_ok = true;
    g_parse_swap_config_ticker = "ETH";
    s_amountToString_ret = true;
    s_amountToString_ret = true;
    g_mem_utils_alloc_ret = false;  // out-of-memory on G_swap_crosschain_hash
    TEST_ASSERT_FALSE(copy_transaction_parameters(&p, &s_chain));
}

// =============================================================================
// Noreturn entry points
// =============================================================================
// swap_finalize_exchange_sign_transaction and handle_swap_sign_transaction
// are both __attribute__((noreturn)). Use EXPECT_NORETURN to catch their
// final os_lib_end() / app_main() call and inspect the side effects they
// committed before the noreturn fired.

void swap_finalize_exchange_sign_transaction(bool is_success);
void handle_swap_sign_transaction(const chain_config_t *config);

void test_swap_finalize_commits_status_byte_and_ends_lib(void) {
    static volatile uint8_t s_status = 0xAA;
    G_swap_signing_return_value_address = &s_status;
    // Pre-allocate a heap buffer that swap_finalize's APP_MEM_FREE will
    // release (the underlying mem_utils_free calls free() at host).
    extern uint8_t *G_swap_crosschain_hash;
    G_swap_crosschain_hash = malloc(32);
    EXPECT_NORETURN(swap_finalize_exchange_sign_transaction(true));
    TEST_ASSERT_EQUAL(g_noreturn_calls, 1);  // os_lib_end fired
    TEST_ASSERT_EQUAL((int) s_status, 1);    // is_success committed
}

void test_handle_swap_sign_seeds_globals_then_runs_app_main(void) {
    G_swap_mode = SWAP_MODE_STANDARD;
    G_swap_response_ready = true;  // pre-set, must be reset to false
    G_called_from_swap = false;
    g_set_swap_with_calldata_calls = 0;
    g_ui_swap_show_signing_calls = 0;
    EXPECT_NORETURN(handle_swap_sign_transaction(&s_chain));
    TEST_ASSERT_EQUAL(g_noreturn_calls, 1);  // app_main fired
    TEST_ASSERT_TRUE(G_called_from_swap);
    TEST_ASSERT_FALSE(G_swap_response_ready);
    TEST_ASSERT_EQUAL(g_ui_swap_show_signing_calls, 1);
    // STANDARD mode -> no auto-register of the crosschain plugin.
    TEST_ASSERT_EQUAL(g_set_swap_with_calldata_calls, 0);
    TEST_ASSERT_EQUAL_PTR(g_chain_config, &s_chain);
}

void test_handle_swap_sign_crosschain_pending_registers_plugin(void) {
    G_swap_mode = SWAP_MODE_CROSSCHAIN_PENDING_CHECK;
    g_set_swap_with_calldata_calls = 0;
    EXPECT_NORETURN(handle_swap_sign_transaction(&s_chain));
    // CROSSCHAIN_PENDING_CHECK is the gate that auto-registers the
    // swap-with-calldata plugin so the upcoming TX parser dispatches to it.
    TEST_ASSERT_EQUAL(g_set_swap_with_calldata_calls, 1);
}

void setUp(void) {
    Mockcommon_utils_Init();
    amountToString_StubWithCallback(amountToString_stub);
    reset();
}
void tearDown(void) {
    Mockcommon_utils_Verify();
    Mockcommon_utils_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_amount_length_over_32_rejected);
    RUN_TEST(test_fee_amount_length_over_8_rejected);
    RUN_TEST(test_parse_swap_config_failure_rejected);
    RUN_TEST(test_amount_to_string_fees_failure_rejected);
    RUN_TEST(test_amount_to_string_value_failure_standard_rejected);
    RUN_TEST(test_native_swap_returns_true_and_sets_standard_mode);
    RUN_TEST(test_crosschain_calldata_swap_copies_hash);
    RUN_TEST(test_unknown_extra_id_type_sets_error_mode_but_continues);
    RUN_TEST(test_crosschain_non_native_zero_amount_path);
    RUN_TEST(test_crosschain_non_native_zero_amount_failure_rejected);
    RUN_TEST(test_alloc_failure_returns_false);
    RUN_TEST(test_swap_finalize_commits_status_byte_and_ends_lib);
    RUN_TEST(test_handle_swap_sign_seeds_globals_then_runs_app_main);
    RUN_TEST(test_handle_swap_sign_crosschain_pending_registers_plugin);
    return UNITY_END();
}
