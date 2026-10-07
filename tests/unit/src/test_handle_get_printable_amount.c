/**
 * @file test_handle_get_printable_amount.c
 * @brief Unit tests for handle_get_printable_amount at
 *        src/swap/handle_get_printable_amount.c.
 *
 * The Ledger Exchange app calls handle_get_printable_amount when it
 * needs the Ethereum app to format an amount + ticker for display
 * (`"1.5 ETH"`, `"100 USDC"`). The handler writes into the shared
 * params->printable_amount buffer; on any failure path the buffer must
 * be left zeroed so the host doesn't display stale / wrong text.
 *
 * Pin every branch:
 *  - amount_length > 32              params->printable_amount stays zero
 *  - parse_swap_config fails         same
 *  - amountToString fails            same (the dispatcher re-zeros the
 *                                     buffer after the failed write)
 *  - happy path                      params->printable_amount holds the
 *                                     formatted string
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "swap_lib_calls.h"
#include "chain_config.h"
#include "eth_swap_utils.h"
#include "handle_get_printable_amount.h"
#include "Mockcommon_utils.h"

static bool g_amountToString_ret = true;
static bool g_parse_swap_config_ret = true;

// =============================================================================
// Wraps
// =============================================================================

bool parse_swap_config(const uint8_t *config, uint8_t config_size, swap_context_t *ctx) {
    (void) config;
    (void) config_size;
    (void) ctx;
    return (bool) g_parse_swap_config_ret;
}

void get_asset_info_on_network(bool is_fee,
                               swap_context_t *context,
                               chain_config_t *chain_config,
                               char **ticker,
                               uint8_t *decimals) {
    (void) is_fee;
    (void) context;
    (void) chain_config;
    if (ticker != NULL) {
        // Static literal: the unit under test only passes the pointer
        // through to amountToString (also wrapped) so storage lifetime
        // doesn't matter.
        static char fake[] = "ETH";
        *ticker = fake;
    }
    if (decimals != NULL) {
        *decimals = 18;
    }
}

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
    bool ok = g_amountToString_ret;
    if (ok && out_buffer != NULL && out_buffer_size > 0) {
        strlcpy(out_buffer, "1 ETH", out_buffer_size);
    }
    return ok;
}

// =============================================================================
// Fixture
// =============================================================================

static chain_config_t s_chain = {.ticker = "ETH", .chain_id = 1, .coin_type = 60};
static uint8_t s_amount[32];
static uint8_t s_coin_cfg[16];

static get_printable_amount_parameters_t make_params(void) {
    get_printable_amount_parameters_t p = {0};
    p.coin_configuration = s_coin_cfg;
    p.coin_configuration_length = sizeof(s_coin_cfg);
    p.amount = s_amount;
    p.amount_length = 4;
    p.is_fee = false;
    return p;
}

static void reset(void) {
    memset(s_amount, 0, sizeof(s_amount));
    memset(s_coin_cfg, 0, sizeof(s_coin_cfg));
}

// =============================================================================
// Failure paths -- buffer must stay zero
// =============================================================================

void test_amount_length_over_32_zeros_buffer(void) {
    get_printable_amount_parameters_t p = make_params();
    memset(p.printable_amount, 0xCC, sizeof(p.printable_amount));  // pre-poison
    p.amount_length = 33;
    // parse_swap_config / amountToString MUST NOT be reached.
    handle_get_printable_amount(&p, &s_chain);
    static const char zero[MAX_PRINTABLE_AMOUNT_SIZE] = {0};
    TEST_ASSERT_EQUAL_MEMORY(p.printable_amount, zero, sizeof(p.printable_amount));
}

void test_parse_swap_config_failure_zeros_buffer(void) {
    get_printable_amount_parameters_t p = make_params();
    memset(p.printable_amount, 0xCC, sizeof(p.printable_amount));
    g_parse_swap_config_ret = false;
    handle_get_printable_amount(&p, &s_chain);
    static const char zero[MAX_PRINTABLE_AMOUNT_SIZE] = {0};
    TEST_ASSERT_EQUAL_MEMORY(p.printable_amount, zero, sizeof(p.printable_amount));
}

void test_amount_to_string_failure_zeros_buffer(void) {
    get_printable_amount_parameters_t p = make_params();
    memset(p.printable_amount, 0xCC, sizeof(p.printable_amount));
    g_parse_swap_config_ret = true;
    g_amountToString_ret = false;
    handle_get_printable_amount(&p, &s_chain);
    // The dispatcher re-zeros the buffer on amountToString failure --
    // a partial write would expose stale bytes.
    static const char zero[MAX_PRINTABLE_AMOUNT_SIZE] = {0};
    TEST_ASSERT_EQUAL_MEMORY(p.printable_amount, zero, sizeof(p.printable_amount));
}

// =============================================================================
// Happy path
// =============================================================================

void test_success_writes_formatted_amount(void) {
    get_printable_amount_parameters_t p = make_params();
    g_parse_swap_config_ret = true;
    g_amountToString_ret = true;
    handle_get_printable_amount(&p, &s_chain);
    TEST_ASSERT_EQUAL_STRING(p.printable_amount, "1 ETH");
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
    RUN_TEST(test_amount_length_over_32_zeros_buffer);
    RUN_TEST(test_parse_swap_config_failure_zeros_buffer);
    RUN_TEST(test_amount_to_string_failure_zeros_buffer);
    RUN_TEST(test_success_writes_formatted_amount);
    return UNITY_END();
}
