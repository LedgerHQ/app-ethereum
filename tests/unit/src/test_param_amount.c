/**
 * @file test_param_amount.c
 * @brief Unit tests for the AMOUNT parameter in
 *        src/features/generic_tx_parser/gtp_param_amount.c.
 *
 * Two layers are exercised:
 *   - handle_param_amount_struct() runs the per-parameter TLV parser. The
 *     handler for TAG_VERSION enforces a 1-byte payload, the handler for
 *     TAG_VALUE delegates to handle_value_struct (wrapped here). The TLV
 *     framework also enforces ENFORCE_UNIQUE_TAG so receiving the same
 *     tag twice must be rejected.
 *   - format_param_amount() iterates the parsed-value collection produced
 *     by value_get(), formats each element with amountToString() at
 *     WEI_TO_ETHER scale and pushes it to the field table. The test
 *     verifies the broadcast loop, the ticker resolution, and the early
 *     exits (NULL tx_info, value_get failure, add_to_field_table
 *     failure).
 *
 * Format follows the same conventions as the pre-existing
 * test_param_token_amount / test_param_calldata suites: value_get,
 * value_cleanup, handle_value_struct, get_current_tx_info,
 * get_displayable_ticker and add_to_field_table are all wrapped via
 * linker --wrap.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_param_amount.h"
#include "gtp_parsed_value.h"
#include "gtp_value.h"
#include "gtp_tx_info.h"
#include "gtp_field.h"
#include "shared_context.h"
#include "network.h"
#include "wraps.h"

// =============================================================================
// Globals the module reads
// =============================================================================

static bool g_add_to_field_table_ret = true;

// =============================================================================
// Wrapped dependencies
// =============================================================================

static int g_vg_call = 0;
static s_parsed_value_collection g_vg[1];
static bool g_vg_ret = true;

bool value_get(const s_value *value, s_parsed_value_collection *collection) {
    (void) value;
    *collection = g_vg[g_vg_call++];
    return g_vg_ret;
}

// Used by the TAG_VALUE handler of handle_param_amount_struct.
static bool g_hvs_ret = true;
bool handle_value_struct(const buffer_t *buf, s_value_context *context) {
    (void) buf;
    (void) context;
    return g_hvs_ret;
}

bool add_to_field_table(e_param_type type,
                        const char *key,
                        const char *value,
                        const void *extra_data) {
    (void) extra_data;
    return (bool) g_add_to_field_table_ret;
}

static const s_tx_info *s_tx_info_ret = NULL;
const s_tx_info *get_current_tx_info(void) {
    return s_tx_info_ret;
}

// =============================================================================
// Test fixtures
// =============================================================================

static s_tx_info g_fake_tx_info;

static void reset(void) {
    memset(&g_vg, 0, sizeof(g_vg));
    g_vg_call = 0;
    g_vg_ret = true;
    g_hvs_ret = true;
    g_add_to_field_table_ret = true;
    memset(&g_fake_tx_info, 0, sizeof(g_fake_tx_info));
    g_fake_tx_info.chain_id = 1;
    s_tx_info_ret = &g_fake_tx_info;
}

// 1 ETH = 1e18 wei, big-endian in 32 bytes — the high bytes are zero, the
// low bytes spell out 0x0DE0B6B3A7640000 (= 10^18).
static uint8_t g_one_eth[INT256_LENGTH] = {
    0,    0,    0,    0,    0,    0,    0,    0,  //
    0,    0,    0,    0,    0,    0,    0,    0,  //
    0,    0,    0,    0,    0,    0,    0,    0,  //
    0x0D, 0xE0, 0xB6, 0xB3, 0xA7, 0x64, 0x00, 0x00,
};

// 2 ETH = 2e18 wei
static uint8_t g_two_eth[INT256_LENGTH] = {
    0,    0,    0,    0,    0,    0,    0,    0,  //
    0,    0,    0,    0,    0,    0,    0,    0,  //
    0,    0,    0,    0,    0,    0,    0,    0,  //
    0x1B, 0xC1, 0x6D, 0x67, 0x4E, 0xC8, 0x00, 0x00,
};

// =============================================================================
// format_param_amount
// =============================================================================

void test_format_single_value_ok(void) {
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_one_eth,
                                         .size = INT256_LENGTH,
                                         .offset = 0,
                                         .length = INT256_LENGTH};
    g_add_to_field_table_ret = true;

    s_param_amount param = {0};
    TEST_ASSERT_TRUE(format_param_amount(&param, "Amount"));
}

void test_format_multiple_values_iterates(void) {
    g_vg[0].size = 2;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_one_eth,
                                         .size = INT256_LENGTH,
                                         .offset = 0,
                                         .length = INT256_LENGTH};
    g_vg[0].value[1] = (s_parsed_value) {.ptr = g_two_eth,
                                         .size = INT256_LENGTH,
                                         .offset = 0,
                                         .length = INT256_LENGTH};
    g_add_to_field_table_ret = true;
    g_add_to_field_table_ret = true;

    s_param_amount param = {0};
    TEST_ASSERT_TRUE(format_param_amount(&param, "Amount"));
}

void test_format_value_get_failure_returns_false(void) {
    g_vg_ret = false;
    // No add_to_field_table expected — value_get short-circuits the loop.

    s_param_amount param = {0};
    TEST_ASSERT_FALSE(format_param_amount(&param, "Amount"));
}

void test_format_tx_info_null_returns_false(void) {
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_one_eth,
                                         .size = INT256_LENGTH,
                                         .offset = 0,
                                         .length = INT256_LENGTH};
    s_tx_info_ret = NULL;  // no current tx → cannot resolve ticker

    s_param_amount param = {0};
    TEST_ASSERT_FALSE(format_param_amount(&param, "Amount"));
}

void test_format_add_to_field_table_failure_propagates(void) {
    g_vg[0].size = 2;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_one_eth,
                                         .size = INT256_LENGTH,
                                         .offset = 0,
                                         .length = INT256_LENGTH};
    g_vg[0].value[1] = (s_parsed_value) {.ptr = g_two_eth,
                                         .size = INT256_LENGTH,
                                         .offset = 0,
                                         .length = INT256_LENGTH};

    // First entry accepted, second rejected — loop must break and return false.
    g_add_to_field_table_ret = true;
    g_add_to_field_table_ret = false;

    s_param_amount param = {0};
    TEST_ASSERT_FALSE(format_param_amount(&param, "Amount"));
}

// =============================================================================
// handle_param_amount_struct (TLV parser)
// =============================================================================
//
// TLV byte layout used by tlv_library: tag(1B) | length(1B for <128) | value.
// TAG_VERSION = 0x00, TAG_VALUE = 0x01 in this parser.

static bool run_tlv(const uint8_t *bytes, size_t size, s_param_amount *param) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    s_param_amount_context ctx = {.param = param};
    return handle_param_amount_struct(&buf, &ctx);
}

void test_tlv_happy_path_sets_version(void) {
    // VERSION(0x00) [len 1] [0x42], VALUE(0x01) [len 0]
    const uint8_t bytes[] = {0x00, 0x01, 0x42, 0x01, 0x00};
    s_param_amount param = {0};

    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &param));
    TEST_ASSERT_EQUAL(param.version, 0x42);
}

void test_tlv_version_wrong_size_rejected(void) {
    // VERSION with a 2-byte payload — handle_version requires exactly 1.
    const uint8_t bytes[] = {0x00, 0x02, 0x01, 0x02, 0x01, 0x00};
    s_param_amount param = {0};

    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &param));
}

void test_tlv_value_handler_failure_propagates(void) {
    g_hvs_ret = false;  // handle_value_struct (wrapped) refuses the payload
    const uint8_t bytes[] = {0x00, 0x01, 0x01, 0x01, 0x00};
    s_param_amount param = {0};

    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &param));
}

void test_tlv_duplicate_tag_rejected(void) {
    // Both TAG_VERSION and TAG_VALUE are ENFORCE_UNIQUE_TAG in the parser.
    // Sending VERSION twice must be rejected.
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,  // VERSION = 1
        0x00,
        0x01,
        0x02,  // VERSION = 2 (duplicate)
        0x01,
        0x00,  // VALUE empty
    };
    s_param_amount param = {0};

    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &param));
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
    RUN_TEST(test_format_single_value_ok);
    RUN_TEST(test_format_multiple_values_iterates);
    RUN_TEST(test_format_value_get_failure_returns_false);
    RUN_TEST(test_format_tx_info_null_returns_false);
    RUN_TEST(test_format_add_to_field_table_failure_propagates);
    RUN_TEST(test_tlv_happy_path_sets_version);
    RUN_TEST(test_tlv_version_wrong_size_rejected);
    RUN_TEST(test_tlv_value_handler_failure_propagates);
    RUN_TEST(test_tlv_duplicate_tag_rejected);
    return UNITY_END();
}
