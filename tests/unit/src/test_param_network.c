/**
 * @file test_param_network.c
 * @brief Unit tests for network parameter formatting
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Includes
#include "gtp_field.h"
#include "gtp_value.h"
#include "shared_context.h"
#include "status_words.h"

// Headers for mocked functions
#include "Mocknetwork.h"
#include "common_utils.h"

// Helper macro to create a param_network with constant chain_id
#define CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param_name, chain_id_val)                  \
    uint8_t param_name##_data[8];                                                     \
    for (int i = 0; i < 8; i++) {                                                     \
        param_name##_data[7 - i] = (chain_id_val >> (i * 8)) & 0xFF;                  \
    }                                                                                 \
    s_param_network param_name = {.version = 1,                                       \
                                  .value = {.type_family = TF_UINT,                   \
                                            .source = SOURCE_CONSTANT,                \
                                            .constant = {.size = sizeof(uint64_t)}}}; \
    memcpy(param_name.value.constant.buf, param_name##_data, 8);

// =============================================================================
// Mock state
// =============================================================================

static bool g_add_to_field_table_ret = true;
static const char *g_get_network_as_string_from_chain_id_ret = NULL;

// =============================================================================
// Mock functions
// =============================================================================

/**
 * @brief Mock implementation of add_to_field_table
 */
bool add_to_field_table(e_param_type param_type,
                        const char *name,
                        const char *value,
                        const void *extra_data) {
    (void) param_type;
    (void) name;
    (void) value;
    (void) extra_data;
    return g_add_to_field_table_ret;
}

static bool network_from_chain_id_stub(char *buffer,
                                       size_t buffer_size,
                                       uint64_t chain_id,
                                       int cmock_num_calls) {
    (void) chain_id;
    (void) cmock_num_calls;
    const char *network_name = g_get_network_as_string_from_chain_id_ret;
    if (network_name == NULL) {
        return false;
    }
    strncpy(buffer, network_name, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    return true;
}

// =============================================================================
// Test cases
// =============================================================================

/**
 * @brief Test formatting Ethereum Mainnet (chain_id = 1)
 */
void test_format_network_ethereum_mainnet(void) {
    uint64_t chain_id = 1;
    const char *field_name = "Network";
    const char *network_name = "Ethereum";

    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, chain_id);

    // Mock only external functions
    g_get_network_as_string_from_chain_id_ret = network_name;
    g_add_to_field_table_ret = true;

    // Test
    TEST_ASSERT_TRUE(format_param_network(&param, field_name));
}

/**
 * @brief Test formatting Polygon (chain_id = 137)
 */
void test_format_network_polygon(void) {
    uint64_t chain_id = 137;
    const char *field_name = "Chain";
    const char *network_name = "Polygon";

    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, chain_id);
    g_get_network_as_string_from_chain_id_ret = network_name;
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_network(&param, field_name));
}

/**
 * @brief Test unsupported chain ID (returns error)
 */
void test_format_network_unsupported(void) {
    uint64_t chain_id = 99999;
    const char *field_name = "Network";

    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, chain_id);
    g_get_network_as_string_from_chain_id_ret = NULL;  // Unsupported

    TEST_ASSERT_FALSE(format_param_network(&param, field_name));
}

/**
 * @brief Test chain ID = 0 (invalid per EIP-2294)
 */
void test_format_network_chain_id_zero(void) {
    uint64_t chain_id = 0;
    const char *field_name = "Network";

    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, chain_id);

    // Should fail - chain_id 0 is not valid
    TEST_ASSERT_FALSE(format_param_network(&param, field_name));
}

/**
 * @brief Test maximum valid chain ID per EIP-2294
 */
void test_format_network_max_chain_id(void) {
    const char *field_name = "Network";
    const char *network_name = "Unknown Chain";

    uint64_t max_chain_id = 0x7FFFFFFFFFFFFFDB;
    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, max_chain_id);
    g_get_network_as_string_from_chain_id_ret = network_name;
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_network(&param, field_name));
}

/**
 * @brief Test chain ID exceeding maximum (invalid per EIP-2294)
 */
void test_format_network_chain_id_overflow(void) {
    const char *field_name = "Network";

    uint64_t invalid_chain_id = 0x7FFFFFFFFFFFFFDC;  // Max + 1
    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, invalid_chain_id);

    // Should fail - exceeds EIP-2294 maximum
    TEST_ASSERT_FALSE(format_param_network(&param, field_name));
}

/**
 * @brief Test wrong type family (not TF_UINT)
 */
void test_format_network_wrong_type_family(void) {
    uint64_t chain_id = 1;
    const char *field_name = "Network";

    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, chain_id);
    param.value.type_family = TF_BYTES;  // Wrong type - should be TF_UINT

    // Should fail - wrong type family
    TEST_ASSERT_FALSE(format_param_network(&param, field_name));
}

// =============================================================================
// handle_param_network_struct — TLV dispatch
// =============================================================================

// Real handle_value_struct is in gtp_value.c and is linked here. Empty
// VALUE payload makes the inner parser accept with no tags consumed.
void test_handle_network_struct_version_and_empty_value_ok(void) {
    uint8_t buf_bytes[] = {
        0x00,
        0x01,
        0x07,  // VERSION = 7
        0x01,
        0x00,  // VALUE (empty inner)
    };
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_network param = {0};
    s_param_network_context ctx = {.param = &param};
    TEST_ASSERT_TRUE(handle_param_network_struct(&buf, &ctx));
    TEST_ASSERT_EQUAL(param.version, 7);
}

// format_network_name rejects payloads whose length is not exactly 8
// bytes (uint64_t) — guards downstream u64_from_BE against a partial
// chain_id. The other length errors (zero / overflow) are covered by
// the existing chain_id-zero / chain_id-overflow tests, but the
// length-mismatch branch needs a value collection whose entry has
// length != 8.
void test_format_network_length_mismatch_rejected(void) {
    // Build a param whose constant is only 4 bytes — value_get returns
    // a collection with one entry of length 4, triggering the
    // length != sizeof(uint64_t) rejection.
    uint8_t four_bytes[4] = {0x00, 0x00, 0x00, 0x01};
    s_param_network param = {
        .version = 1,
        .value = {.type_family = TF_UINT, .source = SOURCE_CONSTANT, .constant = {.size = 4}}};
    memcpy(param.value.constant.buf, four_bytes, 4);

    // add_to_field_table must NOT be called.
    TEST_ASSERT_FALSE(format_param_network(&param, "Network"));
}

// =============================================================================
// Test runner
// =============================================================================

void setUp(void) {
    Mocknetwork_Init();
    g_get_network_as_string_from_chain_id_ret = NULL;
    get_network_as_string_from_chain_id_StubWithCallback(network_from_chain_id_stub);
}
void tearDown(void) {
    Mocknetwork_Verify();
    Mocknetwork_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_format_network_ethereum_mainnet);
    RUN_TEST(test_format_network_polygon);
    RUN_TEST(test_format_network_unsupported);
    RUN_TEST(test_format_network_chain_id_zero);
    RUN_TEST(test_format_network_max_chain_id);
    RUN_TEST(test_format_network_chain_id_overflow);
    RUN_TEST(test_format_network_wrong_type_family);
    RUN_TEST(test_handle_network_struct_version_and_empty_value_ok);
    RUN_TEST(test_format_network_length_mismatch_rejected);
    return UNITY_END();
}
