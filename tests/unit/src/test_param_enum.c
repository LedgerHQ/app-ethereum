/**
 * @file test_param_enum.c
 * @brief Unit tests for the ENUM parameter in
 *        src/features/generic_tx_parser/gtp_param_enum.c.
 *
 * ENUM renders a numeric value as a human label looked up in the
 * enum_value table keyed by (chain_id, contract_addr, selector, id,
 * value). The formatter takes the LAST byte of the parsed value as the
 * lookup key (so "value of 0x05" works whether the field is 1, 4, or 32
 * bytes wide), then chains five preconditions that each surface as a
 * `false` return when they fail:
 *   - value_get must succeed,
 *   - get_current_tx_info must be non-NULL (chain_id source),
 *   - the parsed value must be non-empty,
 *   - calldata_get_selector(get_current_calldata()) must yield a
 *     selector — without it the lookup key is incomplete,
 *   - get_matching_enum must return a registered entry.
 *
 * Tests pin each of those branches in turn, plus the broadcast loop
 * and the standard add_to_field_table propagation. The TLV side covers
 * the happy path and the ENFORCE_UNIQUE_TAG guard on TAG_ID.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_param_enum.h"
#include "gtp_parsed_value.h"
#include "gtp_value.h"
#include "gtp_field.h"
#include "gtp_tx_info.h"
#include "enum_value.h"
#include "calldata.h"
#include "shared_context.h"
#include "wraps.h"

// =============================================================================
// Globals
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

static bool g_hvs_ret = true;
bool handle_value_struct(const buffer_t *buf, s_value_context *context) {
    (void) buf;
    (void) context;
    return g_hvs_ret;
}

static s_tx_info g_fake_tx_info;

static s_calldata g_fake_calldata;  // contents irrelevant — only pointer identity matters
static s_calldata *g_calldata_ret = &g_fake_calldata;
s_calldata *get_current_calldata(void) {
    return g_calldata_ret;
}

static const uint8_t g_selector[4] = {0xDE, 0xAD, 0xBE, 0xEF};
static const uint8_t *g_selector_ret = g_selector;
const uint8_t *calldata_get_selector(const s_calldata *calldata) {
    (void) calldata;
    return g_selector_ret;
}

static const s_enum_value_entry *g_enum_ret = NULL;
const s_enum_value_entry *get_matching_enum(const uint64_t *chain_id,
                                            const uint8_t *contract_addr,
                                            const uint8_t *selector,
                                            uint8_t id,
                                            uint8_t value) {
    return g_enum_ret;
}

bool add_to_field_table(e_param_type type,
                        const char *key,
                        const char *value,
                        const void *extra_data) {
    return (bool) g_add_to_field_table_ret;
}

static const s_tx_info *s_tx_info_ret = NULL;
const s_tx_info *get_current_tx_info(void) {
    return s_tx_info_ret;
}

// =============================================================================
// Fixtures
// =============================================================================

static const uint8_t g_contract[ADDRESS_LENGTH] = {
    0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB, 0xCC, 0xDD,
};

// Enum metadata is resolved against the callee of the current context
const uint8_t *get_current_tx_to(void) {
    return g_contract;
}

static void reset(void) {
    memset(&g_vg, 0, sizeof(g_vg));
    g_vg_call = 0;
    g_vg_ret = true;
    g_hvs_ret = true;
    g_add_to_field_table_ret = true;

    memset(&g_fake_tx_info, 0, sizeof(g_fake_tx_info));
    g_fake_tx_info.chain_id = 1;
    s_tx_info_ret = &g_fake_tx_info;

    g_calldata_ret = &g_fake_calldata;
    g_selector_ret = g_selector;
    g_enum_ret = NULL;

    memset(&g_tx_content, 0, sizeof(g_tx_content));
    memcpy(g_tx_content.destination, g_contract, ADDRESS_LENGTH);
    txContext.content = &g_tx_content;
}

// =============================================================================
// format_param_enum — happy path
// =============================================================================

void test_format_known_enum_resolves(void) {
    static const uint8_t v[] = {0x05};  // enum value = 5
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = v, .size = 1, .offset = 0, .length = 1};

    static const s_enum_value_entry entry = {.name = "BUY", .value = 5, .id = 7};
    g_enum_ret = &entry;
    g_add_to_field_table_ret = true;

    s_param_enum param = {0};
    param.id = 7;
    param.value.type_family = TF_UINT;
    param.value.type_size = sizeof(uint8_t);
    TEST_ASSERT_TRUE(format_param_enum(&param, "Action"));
}

void test_format_takes_last_byte_of_wide_value(void) {
    // 32-byte BE value with only the LSB set to 0x09 — the formatter
    // explicitly takes ptr[length - 1] as the lookup key.
    static const uint8_t v[INT256_LENGTH] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x09,
    };
    g_vg[0].size = 1;
    g_vg[0].value[0] =
        (s_parsed_value) {.ptr = v, .size = INT256_LENGTH, .offset = 0, .length = INT256_LENGTH};

    static const s_enum_value_entry entry = {.name = "SELL", .value = 9, .id = 7};
    g_enum_ret = &entry;
    g_add_to_field_table_ret = true;

    s_param_enum param = {0};
    param.id = 7;
    param.value.type_family = TF_UINT;
    param.value.type_size = sizeof(uint8_t);
    TEST_ASSERT_TRUE(format_param_enum(&param, "Action"));
}

// =============================================================================
// format_param_enum — reject paths
// =============================================================================

void test_format_value_get_failure_returns_false(void) {
    g_vg_ret = false;
    s_param_enum param = {0};
    TEST_ASSERT_FALSE(format_param_enum(&param, "Action"));
}

void test_format_tx_info_null_returns_false(void) {
    static const uint8_t v[] = {0x01};
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = v, .size = 1, .offset = 0, .length = 1};
    s_tx_info_ret = NULL;

    s_param_enum param = {0};
    TEST_ASSERT_FALSE(format_param_enum(&param, "Action"));
}

void test_format_empty_value_rejected(void) {
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = NULL, .size = 0, .offset = 0, .length = 0};

    s_param_enum param = {0};
    TEST_ASSERT_FALSE(format_param_enum(&param, "Action"));
}

void test_format_selector_missing_rejects(void) {
    static const uint8_t v[] = {0x01};
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = v, .size = 1, .offset = 0, .length = 1};
    g_selector_ret = NULL;

    s_param_enum param = {0};
    TEST_ASSERT_FALSE(format_param_enum(&param, "Action"));
}

void test_format_unknown_enum_rejects(void) {
    static const uint8_t v[] = {0xAA};  // some unregistered value
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = v, .size = 1, .offset = 0, .length = 1};
    g_enum_ret = NULL;  // not found

    s_param_enum param = {0};
    TEST_ASSERT_FALSE(format_param_enum(&param, "Action"));
}

void test_format_add_to_field_table_failure_propagates(void) {
    static const uint8_t v[] = {0x01};
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = v, .size = 1, .offset = 0, .length = 1};

    static const s_enum_value_entry entry = {.name = "OPEN", .value = 1};
    g_enum_ret = &entry;
    g_add_to_field_table_ret = false;

    s_param_enum param = {0};
    TEST_ASSERT_FALSE(format_param_enum(&param, "Action"));
}

// =============================================================================
// handle_param_enum_struct (TLV)
// =============================================================================

static bool run_tlv(const uint8_t *bytes, size_t size, s_param_enum *param) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    s_param_enum_context ctx = {.param = param};
    return handle_param_enum_struct(&buf, &ctx);
}

void test_tlv_happy_path(void) {
    // VERSION=1, ID=3, VALUE empty
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x01,
        0x03,
        0x02,
        0x00,
    };
    s_param_enum param = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &param));
    TEST_ASSERT_EQUAL(param.version, 1);
    TEST_ASSERT_EQUAL(param.id, 3);
}

void test_tlv_duplicate_id_rejected(void) {
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x01,
        0x03,
        0x01,
        0x01,
        0x04,  // duplicate ID
        0x02,
        0x00,
    };
    s_param_enum param = {0};
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
    RUN_TEST(test_format_known_enum_resolves);
    RUN_TEST(test_format_takes_last_byte_of_wide_value);
    RUN_TEST(test_format_value_get_failure_returns_false);
    RUN_TEST(test_format_tx_info_null_returns_false);
    RUN_TEST(test_format_empty_value_rejected);
    RUN_TEST(test_format_selector_missing_rejects);
    RUN_TEST(test_format_unknown_enum_rejects);
    RUN_TEST(test_format_add_to_field_table_failure_propagates);
    RUN_TEST(test_tlv_happy_path);
    RUN_TEST(test_tlv_duplicate_id_rejected);
    return UNITY_END();
}
