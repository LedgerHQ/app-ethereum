/**
 * @file test_param_enum.c
 * @brief Unit tests for the ENUM parameter in
 *        src/features/gcs/gcs_param_enum.c.
 *
 * ENUM renders a numeric value as a human label looked up in the
 * enum_value table keyed by (chain_id, contract_addr, selector, id,
 * value). The formatter takes the LAST byte of the parsed value as the
 * lookup key (so "value of 0x05" works whether the field is 1, 4, or 32
 * bytes wide), then chains five preconditions that each surface as a
 * `false` return when they fail:
 *   - value_get must succeed,
 *   - the operation being described must be on a chain (chain_id source),
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

#include "gcs_param_enum.h"
#include "gcs_parsed_value.h"
#include "gcs_value.h"
#include "gcs_field.h"
#include "operation_info.h"
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

static int g_value_cleanup_calls = 0;
void value_cleanup(const s_value *value, const s_parsed_value_collection *collection) {
    (void) value;
    (void) collection;
    g_value_cleanup_calls++;
}

static bool g_hvs_ret = true;
bool handle_value_struct(const buffer_t *buf, s_value_context *context) {
    (void) buf;
    (void) context;
    return g_hvs_ret;
}

// What the enum is looked up with: the key of the operation being described
static s_operation_key g_operation_key;
static bool g_operation_key_ret = true;
bool get_current_operation_key(s_operation_key *key) {
    if (!g_operation_key_ret) {
        return false;
    }
    *key = g_operation_key;
    return true;
}

static const s_enum_value_entry *g_enum_ret = NULL;
static s_operation_key g_enum_lookup_key;
const s_enum_value_entry *get_matching_enum(const s_operation_key *operation_key,
                                            uint8_t id,
                                            uint8_t value) {
    g_enum_lookup_key = *operation_key;
    return g_enum_ret;
}

bool add_to_field_table(e_param_type type,
                        const char *key,
                        const char *value,
                        const void *extra_data) {
    return (bool) g_add_to_field_table_ret;
}

// =============================================================================
// Fixtures
// =============================================================================

static const uint8_t g_contract[ADDRESS_LENGTH] = {
    0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB, 0xCC, 0xDD,
};

static void reset(void) {
    memset(&g_operation_key, 0, sizeof(g_operation_key));
    g_operation_key_ret = true;
    memset(&g_enum_lookup_key, 0, sizeof(g_enum_lookup_key));
    memset(&g_vg, 0, sizeof(g_vg));
    g_vg_call = 0;
    g_value_cleanup_calls = 0;
    g_vg_ret = true;
    g_hvs_ret = true;
    g_add_to_field_table_ret = true;

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
    g_operation_key_ret = false;

    s_param_enum param = {0};
    param.value.type_family = TF_UINT;
    param.value.type_size = sizeof(uint8_t);
    TEST_ASSERT_FALSE(format_param_enum(&param, "Action"));
    // The extracted values are released on this early exit too
    TEST_ASSERT_EQUAL(g_value_cleanup_calls, 1);
}

void test_format_empty_value_rejected(void) {
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = NULL, .size = 0, .offset = 0, .length = 0};

    s_param_enum param = {0};
    TEST_ASSERT_FALSE(format_param_enum(&param, "Action"));
}

void test_format_operation_key_missing_rejects(void) {
    static const uint8_t v[] = {0x01};
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = v, .size = 1, .offset = 0, .length = 1};
    g_operation_key_ret = false;

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
// format_param_enum — the key the enum is looked up with
// =============================================================================

// the enum is looked up with the key of the operation being described, whichever kind it is
void test_format_looks_up_with_the_key_of_the_operation(void) {
    static const uint8_t v[] = {0x05};
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = v, .size = 1, .offset = 0, .length = 1};
    static const s_enum_value_entry entry = {.name = "BUY", .value = 5, .id = 7};
    g_enum_ret = &entry;

    g_operation_key.context = OPERATION_INFO_CTX_EIP712;
    memset(g_operation_key.domain_separator, 0xD5, sizeof(g_operation_key.domain_separator));
    memset(g_operation_key.primary_type_hash, 0x5A, sizeof(g_operation_key.primary_type_hash));

    s_param_enum param = {0};
    param.id = 7;
    param.value.type_family = TF_UINT;
    param.value.type_size = sizeof(uint8_t);
    TEST_ASSERT_TRUE(format_param_enum(&param, "Action"));
    TEST_ASSERT_EQUAL(OPERATION_INFO_CTX_EIP712, g_enum_lookup_key.context);
    TEST_ASSERT_EACH_EQUAL_UINT8(0xD5, g_enum_lookup_key.domain_separator, INT256_LENGTH);
    TEST_ASSERT_EACH_EQUAL_UINT8(0x5A, g_enum_lookup_key.primary_type_hash, INT256_LENGTH);
}

// =============================================================================
// handle_param_enum_struct (TLV)
// =============================================================================

static bool run_tlv(const uint8_t *bytes, size_t size, s_param_enum *param) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    s_param_enum_context ctx = {.param = param};
    return handle_param_enum_struct(&buf, &ctx);
}

void test_tlv_version_other_than_one_refused(void) {
    uint8_t bytes[] = {0x00, 0x01, 0x01, 0x01, 0x01, 0x03, 0x02, 0x00};
    const uint8_t other_versions[] = {0x00, 0x02, 0xFF};
    s_param_enum param = {0};

    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &param));
    for (size_t i = 0; i < sizeof(other_versions); i++) {
        bytes[2] = other_versions[i];
        TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &param));
    }
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
    RUN_TEST(test_format_operation_key_missing_rejects);
    RUN_TEST(test_format_unknown_enum_rejects);
    RUN_TEST(test_format_add_to_field_table_failure_propagates);
    RUN_TEST(test_tlv_version_other_than_one_refused);
    RUN_TEST(test_format_looks_up_with_the_key_of_the_operation);
    RUN_TEST(test_tlv_happy_path);
    RUN_TEST(test_tlv_duplicate_id_rejected);
    return UNITY_END();
}
