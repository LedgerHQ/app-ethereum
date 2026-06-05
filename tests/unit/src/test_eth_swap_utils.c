/**
 * @file test_eth_swap_utils.c
 * @brief Unit tests for the swap-mode integration helpers at
 *        src/swap/eth_swap_utils.c.
 *
 * Three concerns are covered:
 *   - parse_swap_config: the binary configuration the Exchange app hands
 *     the Ethereum app at swap-init time. The buffer carries a swapped-
 *     asset ticker + decimals + 8-byte BE chain_id, and optionally a
 *     fees-asset ticker + decimals (fees decimals must equal
 *     WEI_TO_ETHER). The parser is a security boundary — it must reject
 *     malformed lengths and mismatched fees decimals.
 *   - get_asset_info_on_network: chooses between
 *     context->fees_asset_info, context->swapped_asset_info, or the chain
 *     config ticker (native-currency fallback when the fees asset is absent).
 *   - swap_check_{destination, amount, fee}: NULL-guard + match path.
 *     The mismatch path calls app_exit() on the device (noreturn) and
 *     is therefore not exercised here.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "eth_swap_utils.h"
#include "shared_context.h"
#include "wraps.h"

// =============================================================================
// Globals
// =============================================================================

// =============================================================================
// Stubs
// =============================================================================

// G_io_apdu_buffer is referenced by the send_swap_error_with_string macro
// expansion via sizeof(). Provide a global with the standard 260-byte size.
unsigned char G_io_apdu_buffer[260];

// Stubs for the abort path (not exercised by these tests — the mismatch
// branch calls app_exit() after send_swap_error_with_buffers).
void send_swap_error_with_buffers(uint16_t status_word,
                                  uint8_t common_error_code,
                                  uint8_t application_specific_error_code,
                                  void *string_buffer,
                                  uint8_t buffer_count) {
    (void) status_word;
    (void) common_error_code;
    (void) application_specific_error_code;
    (void) string_buffer;
    (void) buffer_count;
}
// app_exit is declared noreturn in the SDK. The stub must honor that
// contract — if a test ever hits the mismatch path by accident, we want
// it to abort the test process rather than silently fall through to
// unreachable code.
__attribute__((noreturn)) void app_exit(void) {
    fail_msg("app_exit() reached unexpectedly");
    while (1) {
    }
}

// =============================================================================
// Fixture
// =============================================================================

static int reset(void **state) {
    (void) state;
    memset(&strings, 0, sizeof(strings));
    return 0;
}

// =============================================================================
// parse_swap_config — happy paths
// =============================================================================

static void test_parse_with_asset_chain_and_fees(void **state) {
    (void) state;
    // asset_ticker_len=3, "ETH", decimals=18, chain_id=1 (BE),
    // fees_ticker_len=3, "ETH", decimals=18 (WEI_TO_ETHER)
    const uint8_t config[] = {
        0x03,
        'E',
        'T',
        'H',
        0x12,  // asset
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,  // chain_id = 1
        0x03,
        'E',
        'T',
        'H',
        0x12,  // fees
    };
    swap_context_t ctx;
    assert_true(parse_swap_config(config, sizeof(config), &ctx));
    assert_string_equal(ctx.swapped_asset_info.ticker, "ETH");
    assert_int_equal(ctx.swapped_asset_info.decimals, 18);
    assert_int_equal(ctx.chain_id, 1);
    assert_string_equal(ctx.fees_asset_info.ticker, "ETH");
    assert_int_equal(ctx.fees_asset_info.decimals, 18);
}

static void test_parse_without_fees_section_defaults_fees(void **state) {
    (void) state;
    // No fees block — the parser leaves the default fees ticker empty
    // (so get_asset_info_on_network falls back to the chain config ticker)
    // and default fees decimals = WEI_TO_ETHER (set up by explicit_bzero
    // followed by an explicit assignment in the parser).
    const uint8_t config[] = {
        0x04,
        'U',
        'S',
        'D',
        'C',
        0x06,  // asset USDC, 6 dec
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x89,  // chain_id = 137
    };
    swap_context_t ctx;
    assert_true(parse_swap_config(config, sizeof(config), &ctx));
    assert_string_equal(ctx.swapped_asset_info.ticker, "USDC");
    assert_int_equal(ctx.swapped_asset_info.decimals, 6);
    assert_int_equal(ctx.chain_id, 137);
    assert_int_equal(ctx.fees_asset_info.ticker[0], '\0');
    assert_int_equal(ctx.fees_asset_info.decimals, 18 /* WEI_TO_ETHER */);
}

// =============================================================================
// parse_swap_config — input validation
// =============================================================================

static void test_parse_null_inputs_rejected(void **state) {
    (void) state;
    swap_context_t ctx;
    const uint8_t buf[1] = {0};
    assert_false(parse_swap_config(NULL, 1, &ctx));
    assert_false(parse_swap_config(buf, 0, &ctx));
    assert_false(parse_swap_config(buf, 1, NULL));
}

static void test_parse_ticker_len_zero_rejected(void **state) {
    (void) state;
    const uint8_t config[] = {0x00};
    swap_context_t ctx;
    assert_false(parse_swap_config(config, sizeof(config), &ctx));
}

static void test_parse_ticker_len_oversized_rejected(void **state) {
    (void) state;
    // MAX_TICKER_LEN == 51, the parser rejects (MAX_TICKER_LEN - 2)+ which
    // is 50+. A ticker length of 50 is also rejected per the > check.
    const uint8_t config[] = {50};
    swap_context_t ctx;
    assert_false(parse_swap_config(config, sizeof(config), &ctx));
}

static void test_parse_truncated_ticker_rejected(void **state) {
    (void) state;
    // Declares 5 bytes of ticker but only 2 actually follow.
    const uint8_t config[] = {0x05, 'A', 'B'};
    swap_context_t ctx;
    assert_false(parse_swap_config(config, sizeof(config), &ctx));
}

static void test_parse_missing_decimals_rejected(void **state) {
    (void) state;
    // Ticker is fine but no decimals byte after.
    const uint8_t config[] = {0x03, 'E', 'T', 'H'};
    swap_context_t ctx;
    assert_false(parse_swap_config(config, sizeof(config), &ctx));
}

static void test_parse_missing_chain_id_rejected(void **state) {
    (void) state;
    // Asset complete, but no 8-byte chain_id.
    const uint8_t config[] = {0x03, 'E', 'T', 'H', 0x12, 0x00, 0x00, 0x00};
    swap_context_t ctx;
    assert_false(parse_swap_config(config, sizeof(config), &ctx));
}

static void test_parse_invalid_fees_decimals_rejected(void **state) {
    (void) state;
    // Fees decimals != WEI_TO_ETHER → security reject. The Exchange app
    // is supposed to send fees in native units (18 decimals).
    const uint8_t config[] = {
        0x03,
        'E',
        'T',
        'H',
        0x12,  // asset
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x03,
        'E',
        'T',
        'H',
        0x06,  // fees decimals = 6 (wrong!)
    };
    swap_context_t ctx;
    assert_false(parse_swap_config(config, sizeof(config), &ctx));
}

// =============================================================================
// get_asset_info_on_network
// =============================================================================

static void test_asset_info_non_fee_uses_swapped(void **state) {
    (void) state;
    swap_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strlcpy(ctx.swapped_asset_info.ticker, "USDC", sizeof(ctx.swapped_asset_info.ticker));
    ctx.swapped_asset_info.decimals = 6;

    char *ticker = NULL;
    uint8_t decimals = 0;
    get_asset_info_on_network(false, &ctx, &g_chainConfig, &ticker, &decimals);
    assert_string_equal(ticker, "USDC");
    assert_int_equal(decimals, 6);
}

static void test_asset_info_fee_with_fees_ticker_uses_it(void **state) {
    (void) state;
    swap_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strlcpy(ctx.fees_asset_info.ticker, "MATIC", sizeof(ctx.fees_asset_info.ticker));
    ctx.fees_asset_info.decimals = 18;

    char *ticker = NULL;
    uint8_t decimals = 0;
    get_asset_info_on_network(true, &ctx, &g_chainConfig, &ticker, &decimals);
    assert_string_equal(ticker, "MATIC");
    assert_int_equal(decimals, 18);
}

static void test_asset_info_fee_empty_ticker_falls_back_to_config(void **state) {
    (void) state;
    // When the swap config omits the fees asset (e.g. ERC20 swap on the native
    // chain), the fee is the chain's native currency: fall back to the chain
    // config ticker. Static networks no longer exist as an intermediate source.
    swap_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.fees_asset_info.decimals = 18;
    strlcpy(g_chainConfig.ticker, "POL", sizeof(g_chainConfig.ticker));

    char *ticker = NULL;
    get_asset_info_on_network(true, &ctx, &g_chainConfig, &ticker, NULL);
    assert_string_equal(ticker, "POL");
}

// =============================================================================
// swap_check_destination / amount / fee — NULL + match
// =============================================================================

static void test_swap_check_destination_null_rejected(void **state) {
    (void) state;
    assert_false(swap_check_destination(NULL));
}

static void test_swap_check_destination_match(void **state) {
    (void) state;
    strlcpy(strings.common.toAddress, "0xAbC123", sizeof(strings.common.toAddress));
    // Case-insensitive via the local strcasecmp_workaround.
    assert_true(swap_check_destination("0xabc123"));
    assert_true(swap_check_destination("0xABC123"));
}

static void test_swap_check_amount_null_rejected(void **state) {
    (void) state;
    assert_false(swap_check_amount(NULL));
}

static void test_swap_check_amount_match(void **state) {
    (void) state;
    strlcpy(strings.common.fullAmount, "1 ETH", sizeof(strings.common.fullAmount));
    assert_true(swap_check_amount("1 ETH"));
}

static void test_swap_check_fee_null_rejected(void **state) {
    (void) state;
    assert_false(swap_check_fee(NULL));
}

static void test_swap_check_fee_match(void **state) {
    (void) state;
    strlcpy(strings.common.maxFee, "0.001 ETH", sizeof(strings.common.maxFee));
    assert_true(swap_check_fee("0.001 ETH"));
}

// =============================================================================
// Runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_parse_with_asset_chain_and_fees, reset),
        cmocka_unit_test_setup(test_parse_without_fees_section_defaults_fees, reset),
        cmocka_unit_test_setup(test_parse_null_inputs_rejected, reset),
        cmocka_unit_test_setup(test_parse_ticker_len_zero_rejected, reset),
        cmocka_unit_test_setup(test_parse_ticker_len_oversized_rejected, reset),
        cmocka_unit_test_setup(test_parse_truncated_ticker_rejected, reset),
        cmocka_unit_test_setup(test_parse_missing_decimals_rejected, reset),
        cmocka_unit_test_setup(test_parse_missing_chain_id_rejected, reset),
        cmocka_unit_test_setup(test_parse_invalid_fees_decimals_rejected, reset),
        cmocka_unit_test_setup(test_asset_info_non_fee_uses_swapped, reset),
        cmocka_unit_test_setup(test_asset_info_fee_with_fees_ticker_uses_it, reset),
        cmocka_unit_test_setup(test_asset_info_fee_empty_ticker_falls_back_to_config, reset),
        cmocka_unit_test_setup(test_swap_check_destination_null_rejected, reset),
        cmocka_unit_test_setup(test_swap_check_destination_match, reset),
        cmocka_unit_test_setup(test_swap_check_amount_null_rejected, reset),
        cmocka_unit_test_setup(test_swap_check_amount_match, reset),
        cmocka_unit_test_setup(test_swap_check_fee_null_rejected, reset),
        cmocka_unit_test_setup(test_swap_check_fee_match, reset),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
