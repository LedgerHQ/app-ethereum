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
 *     context->fees_asset_info, context->swapped_asset_info, or the
 *     displayable ticker for the chain depending on (is_fee,
 *     fees_ticker_present, chain_id) shape.
 *   - swap_check_{destination, amount, fee}: NULL-guard + match path.
 *   - swap_check_value: the value bound at signature time. Which form
 *     is expected is driven by G_swap_expected_value_check, and the
 *     comparison is on the raw bytes, so both the right-alignment of a
 *     minimally-encoded value and the ticker binding that follows a raw
 *     match are covered.
 *
 * The mismatch paths end in a noreturn (app_exit or os_sched_exit after
 * the error is reported to Exchange); they run under EXPECT_NORETURN(),
 * which arms the longjmp in mocks/system_stubs.c.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "eth_swap_utils.h"
#include "shared_context.h"
#include "apdu_constants.h"
#include "buffer.h"
#include "swap_error_code_helpers.h"
#include "wraps.h"

// =============================================================================
// Globals
// =============================================================================

// =============================================================================
// Wraps
// =============================================================================

static const char *g_displayable_ticker_ret = "ETH";
const char *get_displayable_ticker(const uint64_t *chain_id,
                                   const chain_config_t *config,
                                   bool mainnet_only) {
    (void) chain_id;
    (void) config;
    (void) mainnet_only;
    return g_displayable_ticker_ret;
}

// The real helper reports the error to Exchange and terminates the app, which
// makes the app_exit / os_sched_exit written after each call site unreachable.
// The stub records what Exchange would have been told and then takes the same
// longjmp as the other noreturn stubs in mocks/system_stubs.c, so a test drives
// it through EXPECT_NORETURN().
static uint16_t g_swap_error_status_word;
static uint8_t g_swap_error_code;
static char g_swap_error_string[sizeof(G_io_apdu_buffer)];
static int g_swap_error_calls;

void send_swap_error_with_buffers(uint16_t status_word,
                                  uint8_t common_error_code,
                                  uint8_t application_specific_error_code,
                                  const buffer_t *buffer_data,
                                  size_t count) {
    (void) application_specific_error_code;
    g_swap_error_calls++;
    g_swap_error_status_word = status_word;
    g_swap_error_code = common_error_code;
    g_swap_error_string[0] = '\0';
    if ((count == 1) && (buffer_data != NULL) && (buffer_data->ptr != NULL)) {
        size_t len = buffer_data->size;
        if (len >= sizeof(g_swap_error_string)) {
            len = sizeof(g_swap_error_string) - 1;
        }
        memcpy(g_swap_error_string, buffer_data->ptr, len);
        g_swap_error_string[len] = '\0';
    }
    g_noreturn_calls++;
    if (g_noreturn_armed) longjmp(g_noreturn_jmp, 1);
    while (1) {
    }
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    memset(&strings, 0, sizeof(strings));
    g_displayable_ticker_ret = "ETH";
    G_swap_expected_value_check = SWAP_VALUE_CHECK_AMOUNT;
    memset(G_swap_expected_value, 0, sizeof(G_swap_expected_value));
    g_swap_error_calls = 0;
    g_swap_error_status_word = 0;
    g_swap_error_code = 0;
    g_swap_error_string[0] = '\0';
}

// Stage the amount Exchange validated the way copy_transaction_parameters()
// does: raw big-endian bytes, right-aligned in a full word.
static void expect_amount(const uint8_t *value, size_t len) {
    G_swap_expected_value_check = SWAP_VALUE_CHECK_AMOUNT;
    memset(G_swap_expected_value, 0, sizeof(G_swap_expected_value));
    memcpy(G_swap_expected_value + sizeof(G_swap_expected_value) - len, value, len);
}

// =============================================================================
// parse_swap_config — happy paths
// =============================================================================

void test_parse_with_asset_chain_and_fees(void) {
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
    TEST_ASSERT_TRUE(parse_swap_config(config, sizeof(config), &ctx));
    TEST_ASSERT_EQUAL_STRING(ctx.swapped_asset_info.ticker, "ETH");
    TEST_ASSERT_EQUAL(ctx.swapped_asset_info.decimals, 18);
    TEST_ASSERT_EQUAL(ctx.chain_id, 1);
    TEST_ASSERT_EQUAL_STRING(ctx.fees_asset_info.ticker, "ETH");
    TEST_ASSERT_EQUAL(ctx.fees_asset_info.decimals, 18);
}

void test_parse_without_fees_section_defaults_fees(void) {
    // No fees block — the parser leaves the default fees ticker empty
    // (so get_asset_info_on_network falls back to get_displayable_ticker)
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
    TEST_ASSERT_TRUE(parse_swap_config(config, sizeof(config), &ctx));
    TEST_ASSERT_EQUAL_STRING(ctx.swapped_asset_info.ticker, "USDC");
    TEST_ASSERT_EQUAL(ctx.swapped_asset_info.decimals, 6);
    TEST_ASSERT_EQUAL(ctx.chain_id, 137);
    TEST_ASSERT_EQUAL(ctx.fees_asset_info.ticker[0], '\0');
    TEST_ASSERT_EQUAL(ctx.fees_asset_info.decimals, 18 /* WEI_TO_ETHER */);
}

// =============================================================================
// parse_swap_config — input validation
// =============================================================================

void test_parse_null_inputs_rejected(void) {
    swap_context_t ctx;
    const uint8_t buf[1] = {0};
    TEST_ASSERT_FALSE(parse_swap_config(NULL, 1, &ctx));
    TEST_ASSERT_FALSE(parse_swap_config(buf, 0, &ctx));
    TEST_ASSERT_FALSE(parse_swap_config(buf, 1, NULL));
}

void test_parse_ticker_len_zero_rejected(void) {
    const uint8_t config[] = {0x00};
    swap_context_t ctx;
    TEST_ASSERT_FALSE(parse_swap_config(config, sizeof(config), &ctx));
}

void test_parse_ticker_len_oversized_rejected(void) {
    // MAX_TICKER_LEN == 51, the parser rejects (MAX_TICKER_LEN - 2)+ which
    // is 50+. A ticker length of 50 is also rejected per the > check.
    const uint8_t config[] = {50};
    swap_context_t ctx;
    TEST_ASSERT_FALSE(parse_swap_config(config, sizeof(config), &ctx));
}

void test_parse_truncated_ticker_rejected(void) {
    // Declares 5 bytes of ticker but only 2 actually follow.
    const uint8_t config[] = {0x05, 'A', 'B'};
    swap_context_t ctx;
    TEST_ASSERT_FALSE(parse_swap_config(config, sizeof(config), &ctx));
}

void test_parse_missing_decimals_rejected(void) {
    // Ticker is fine but no decimals byte after.
    const uint8_t config[] = {0x03, 'E', 'T', 'H'};
    swap_context_t ctx;
    TEST_ASSERT_FALSE(parse_swap_config(config, sizeof(config), &ctx));
}

void test_parse_missing_chain_id_rejected(void) {
    // Asset complete, but no 8-byte chain_id.
    const uint8_t config[] = {0x03, 'E', 'T', 'H', 0x12, 0x00, 0x00, 0x00};
    swap_context_t ctx;
    TEST_ASSERT_FALSE(parse_swap_config(config, sizeof(config), &ctx));
}

void test_parse_invalid_fees_decimals_rejected(void) {
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
    TEST_ASSERT_FALSE(parse_swap_config(config, sizeof(config), &ctx));
}

// =============================================================================
// get_asset_info_on_network
// =============================================================================

void test_asset_info_non_fee_uses_swapped(void) {
    swap_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strlcpy(ctx.swapped_asset_info.ticker, "USDC", sizeof(ctx.swapped_asset_info.ticker));
    ctx.swapped_asset_info.decimals = 6;

    char *ticker = NULL;
    uint8_t decimals = 0;
    get_asset_info_on_network(false, &ctx, &g_chainConfig, &ticker, &decimals);
    TEST_ASSERT_EQUAL_STRING(ticker, "USDC");
    TEST_ASSERT_EQUAL(decimals, 6);
}

void test_asset_info_fee_with_fees_ticker_uses_it(void) {
    swap_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strlcpy(ctx.fees_asset_info.ticker, "MATIC", sizeof(ctx.fees_asset_info.ticker));
    ctx.fees_asset_info.decimals = 18;

    char *ticker = NULL;
    uint8_t decimals = 0;
    get_asset_info_on_network(true, &ctx, &g_chainConfig, &ticker, &decimals);
    TEST_ASSERT_EQUAL_STRING(ticker, "MATIC");
    TEST_ASSERT_EQUAL(decimals, 18);
}

void test_asset_info_fee_empty_ticker_uses_displayable(void) {
    swap_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.chain_id = 137;
    ctx.fees_asset_info.decimals = 18;
    g_displayable_ticker_ret = "POL";

    char *ticker = NULL;
    get_asset_info_on_network(true, &ctx, &g_chainConfig, &ticker, NULL);
    TEST_ASSERT_EQUAL_STRING(ticker, "POL");
}

void test_asset_info_fee_empty_ticker_zero_chain_id_falls_back_to_config(void) {
    swap_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.chain_id = 0;  // missing
    g_chainConfig.chain_id = 1;
    g_displayable_ticker_ret = "ETH";

    char *ticker = NULL;
    get_asset_info_on_network(true, &ctx, &g_chainConfig, &ticker, NULL);
    // The parser must have copied g_chainConfig.chain_id into ctx.chain_id
    // as a fallback (so subsequent calls can reuse it).
    TEST_ASSERT_EQUAL(ctx.chain_id, 1);
    TEST_ASSERT_EQUAL_STRING(ticker, "ETH");
}

// =============================================================================
// swap_check_destination / amount / fee — NULL + match
// =============================================================================

void test_swap_check_destination_null_rejected(void) {
    TEST_ASSERT_FALSE(swap_check_destination(NULL));
}

void test_swap_check_destination_match(void) {
    strlcpy(strings.common.toAddress, "0xAbC123", sizeof(strings.common.toAddress));
    // Case-insensitive via the local strcasecmp_workaround.
    TEST_ASSERT_TRUE(swap_check_destination("0xabc123"));
    TEST_ASSERT_TRUE(swap_check_destination("0xABC123"));
}

void test_swap_check_amount_null_rejected(void) {
    TEST_ASSERT_FALSE(swap_check_amount(NULL));
}

void test_swap_check_amount_match(void) {
    strlcpy(strings.common.fullAmount, "1 ETH", sizeof(strings.common.fullAmount));
    TEST_ASSERT_TRUE(swap_check_amount("1 ETH"));
}

void test_swap_check_fee_null_rejected(void) {
    TEST_ASSERT_FALSE(swap_check_fee(NULL));
}

void test_swap_check_fee_match(void) {
    strlcpy(strings.common.maxFee, "0.001 ETH", sizeof(strings.common.maxFee));
    TEST_ASSERT_TRUE(swap_check_fee("0.001 ETH"));
}

// =============================================================================
// swap_check_value
// =============================================================================

// 1e18 wei, minimally encoded the way the RLP parser hands the value over.
static const uint8_t ONE_ETHER[] = {0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00, 0x00};

void test_check_value_null_inputs_rejected(void) {
    const uint8_t raw[] = {0x00};
    TEST_ASSERT_FALSE(swap_check_value(NULL, raw, sizeof(raw)));
    TEST_ASSERT_FALSE(swap_check_value("0 ETH", NULL, 0));
    // A rejected input is a plain false: nothing is reported to Exchange.
    TEST_ASSERT_EQUAL(0, g_swap_error_calls);
}

void test_check_value_longer_than_a_word_aborts(void) {
    uint8_t raw[INT256_LENGTH + 1];
    memset(raw, 0, sizeof(raw));
    expect_amount(raw, 1);

    // Fails closed rather than skipping the check, even though the value is zero
    // and the expectation would match on a shorter encoding.
    EXPECT_NORETURN(swap_check_value("0 ETH", raw, sizeof(raw)));
}

void test_check_value_zero_accepts_empty_encoding(void) {
    const uint8_t raw[] = {0x00};
    G_swap_expected_value_check = SWAP_VALUE_CHECK_ZERO;

    // RLP encodes a zero value as an empty string, so the parser reports length 0.
    TEST_ASSERT_TRUE(swap_check_value("0 ETH", raw, 0));
}

void test_check_value_zero_accepts_explicit_zero_bytes(void) {
    const uint8_t raw[] = {0x00, 0x00, 0x00};
    G_swap_expected_value_check = SWAP_VALUE_CHECK_ZERO;

    TEST_ASSERT_TRUE(swap_check_value("0 ETH", raw, sizeof(raw)));
}

void test_check_value_zero_rejects_nonzero(void) {
    G_swap_expected_value_check = SWAP_VALUE_CHECK_ZERO;

    // Native currency spent on top of the token the user validated in Exchange.
    EXPECT_NORETURN(swap_check_value("1 ETH", ONE_ETHER, sizeof(ONE_ETHER)));
    TEST_ASSERT_EQUAL(1, g_swap_error_calls);
    TEST_ASSERT_EQUAL(APDU_RESPONSE_MODE_CHECK_FAILED, g_swap_error_status_word);
    TEST_ASSERT_EQUAL(SWAP_EC_ERROR_WRONG_AMOUNT, g_swap_error_code);
}

void test_check_value_amount_match(void) {
    expect_amount(ONE_ETHER, sizeof(ONE_ETHER));
    strlcpy(strings.common.fullAmount, "1 ETH", sizeof(strings.common.fullAmount));

    TEST_ASSERT_TRUE(swap_check_value("1 ETH", ONE_ETHER, sizeof(ONE_ETHER)));
}

void test_check_value_amount_match_ignores_leading_zeros(void) {
    uint8_t padded[INT256_LENGTH];
    memset(padded, 0, sizeof(padded));
    memcpy(padded + sizeof(padded) - sizeof(ONE_ETHER), ONE_ETHER, sizeof(ONE_ETHER));
    expect_amount(ONE_ETHER, sizeof(ONE_ETHER));
    strlcpy(strings.common.fullAmount, "1 ETH", sizeof(strings.common.fullAmount));

    // Same amount, encoded on a full word: right-aligning both sides makes the
    // check independent of how many bytes each side was encoded with.
    TEST_ASSERT_TRUE(swap_check_value("1 ETH", padded, sizeof(padded)));
}

void test_check_value_amount_rejects_other_amount(void) {
    const uint8_t two_ether[] = {0x1b, 0xc1, 0x6d, 0x67, 0x4e, 0xc8, 0x00, 0x00};
    expect_amount(ONE_ETHER, sizeof(ONE_ETHER));
    strlcpy(strings.common.fullAmount, "1 ETH", sizeof(strings.common.fullAmount));

    EXPECT_NORETURN(swap_check_value("2 ETH", two_ether, sizeof(two_ether)));
    TEST_ASSERT_EQUAL(1, g_swap_error_calls);
    TEST_ASSERT_EQUAL(APDU_RESPONSE_MODE_CHECK_FAILED, g_swap_error_status_word);
    TEST_ASSERT_EQUAL(SWAP_EC_ERROR_WRONG_AMOUNT, g_swap_error_code);
}

void test_check_value_amount_rejects_same_string_other_scale(void) {
    // 1e6: what "1 USDC" is worth at 6 decimals. Exchange validated one native
    // unit, so the formatted forms collide while the raw values do not — the
    // very collision the raw comparison exists to catch.
    const uint8_t one_at_six_decimals[] = {0x0f, 0x42, 0x40};
    expect_amount(ONE_ETHER, sizeof(ONE_ETHER));
    strlcpy(strings.common.fullAmount, "1 USDC", sizeof(strings.common.fullAmount));

    EXPECT_NORETURN(swap_check_value("1 USDC", one_at_six_decimals, sizeof(one_at_six_decimals)));
    TEST_ASSERT_EQUAL(SWAP_EC_ERROR_WRONG_AMOUNT, g_swap_error_code);
}

void test_check_value_amount_rejects_ticker_mismatch(void) {
    expect_amount(ONE_ETHER, sizeof(ONE_ETHER));
    strlcpy(strings.common.fullAmount, "1 ETH", sizeof(strings.common.fullAmount));

    // Raw bytes match; only the asset the two sides spell the amount with differs,
    // which the formatted comparison delegated to swap_check_amount catches.
    EXPECT_NORETURN(swap_check_value("1 POL", ONE_ETHER, sizeof(ONE_ETHER)));
    TEST_ASSERT_EQUAL(SWAP_EC_ERROR_WRONG_AMOUNT, g_swap_error_code);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_with_asset_chain_and_fees);
    RUN_TEST(test_parse_without_fees_section_defaults_fees);
    RUN_TEST(test_parse_null_inputs_rejected);
    RUN_TEST(test_parse_ticker_len_zero_rejected);
    RUN_TEST(test_parse_ticker_len_oversized_rejected);
    RUN_TEST(test_parse_truncated_ticker_rejected);
    RUN_TEST(test_parse_missing_decimals_rejected);
    RUN_TEST(test_parse_missing_chain_id_rejected);
    RUN_TEST(test_parse_invalid_fees_decimals_rejected);
    RUN_TEST(test_asset_info_non_fee_uses_swapped);
    RUN_TEST(test_asset_info_fee_with_fees_ticker_uses_it);
    RUN_TEST(test_asset_info_fee_empty_ticker_uses_displayable);
    RUN_TEST(test_asset_info_fee_empty_ticker_zero_chain_id_falls_back_to_config);
    RUN_TEST(test_swap_check_destination_null_rejected);
    RUN_TEST(test_swap_check_destination_match);
    RUN_TEST(test_swap_check_amount_null_rejected);
    RUN_TEST(test_swap_check_amount_match);
    RUN_TEST(test_swap_check_fee_null_rejected);
    RUN_TEST(test_swap_check_fee_match);
    RUN_TEST(test_check_value_null_inputs_rejected);
    RUN_TEST(test_check_value_longer_than_a_word_aborts);
    RUN_TEST(test_check_value_zero_accepts_empty_encoding);
    RUN_TEST(test_check_value_zero_accepts_explicit_zero_bytes);
    RUN_TEST(test_check_value_zero_rejects_nonzero);
    RUN_TEST(test_check_value_amount_match);
    RUN_TEST(test_check_value_amount_match_ignores_leading_zeros);
    RUN_TEST(test_check_value_amount_rejects_other_amount);
    RUN_TEST(test_check_value_amount_rejects_same_string_other_scale);
    RUN_TEST(test_check_value_amount_rejects_ticker_mismatch);
    return UNITY_END();
}
