/**
 * @file test_param_trusted_name.c
 * @brief Unit tests for trusted name parameter formatting with constraints
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// Includes
#include "gtp_field.h"
#include "gtp_value.h"
#include "shared_context.h"
#include "status_words.h"

#include "os.h"

// Headers for mocked functions
#include "Mocktrusted_name.h"
#include "address_name_lookup.h"
#include "utils.h"
#include "get_public_key.h"
#include "tx_ctx.h"
#include "common_utils.h"
#include "Mocknetwork.h"

// Helper macro to create an Ethereum address (20 bytes)
#define CREATE_ADDRESS_PARAM(param_name, ...)                                           \
    uint8_t param_name##_addr[ADDRESS_LENGTH] = {__VA_ARGS__};                          \
    s_param_trusted_name param_name = {.version = 1,                                    \
                                       .value = {.type_family = TF_BYTES,               \
                                                 .source = SOURCE_CONSTANT,             \
                                                 .constant = {.size = ADDRESS_LENGTH}}, \
                                       .type_count = 1,                                 \
                                       .types = {TN_TYPE_ACCOUNT},                      \
                                       .source_count = 1,                               \
                                       .sources = {TN_SOURCE_CAL},                      \
                                       .sender_addr_count = 0};                         \
    memcpy(param_name.value.constant.buf, param_name##_addr, ADDRESS_LENGTH);

// Helper macro to create an EIP-7930 interoperable address param
// Format: [chain_id_byte (1 byte)][EVM address (20 bytes)] = 21 bytes total
#define CREATE_INTEROP_PARAM(param_name, chain_id_byte, ...)                                \
    uint8_t param_name##_eip7930[1 + ADDRESS_LENGTH] = {chain_id_byte, __VA_ARGS__};        \
    s_param_trusted_name param_name = {.version = 1,                                        \
                                       .value = {.type_family = TF_BYTES,                   \
                                                 .source = SOURCE_CONSTANT,                 \
                                                 .constant = {.size = 1 + ADDRESS_LENGTH}}, \
                                       .type_count = 1,                                     \
                                       .types = {TN_TYPE_ACCOUNT},                          \
                                       .source_count = 1,                                   \
                                       .sources = {TN_SOURCE_CAL},                          \
                                       .sender_addr_count = 0,                              \
                                       .value_type = TNVT_INTEROPERABLE};                   \
    memcpy(param_name.value.constant.buf, param_name##_eip7930, sizeof(param_name##_eip7930));

static bool g_add_to_field_table_ret = true;
static int g_get_current_tx_chain_id_ret = 0;
static const char *g_get_network_as_string_from_chain_id_ret = NULL;
static uint16_t g_get_public_key_ret = 0x9000;
static const void *g_get_trusted_name_ret = NULL;

// =============================================================================
// Mock functions
// =============================================================================

/**
 * @brief Mock implementation of add_to_field_table
 */
bool add_to_field_table(e_param_type param_type, const char *name, const char *value) {
    return (bool) g_add_to_field_table_ret;
}

/**
 * @brief Mock implementation of get_current_tx_chain_id
 */
uint64_t get_current_tx_chain_id(void) {
    return (uint64_t) g_get_current_tx_chain_id_ret;
}

static const s_trusted_name *trusted_name_stub(uint8_t type_count,
                                               const e_name_type *types,
                                               uint8_t source_count,
                                               const e_name_source *sources,
                                               const uint64_t *chain_id,
                                               const uint8_t *addr,
                                               int cmock_num_calls) {
    (void) type_count;
    (void) types;
    (void) source_count;
    (void) sources;
    (void) chain_id;
    (void) addr;
    (void) cmock_num_calls;
    return (const s_trusted_name *) g_get_trusted_name_ret;
}

/**
 * @brief Mock implementation of get_address_display_name
 *
 * Delegates to the existing get_trusted_name mock so that the 8 existing tests
 * that set expectations on get_trusted_name continue to work unchanged.
 */
bool get_address_display_name(const uint8_t *addr,
                              uint64_t chain_id,
                              uint8_t type_count,
                              const e_name_type *types,
                              uint8_t source_count,
                              const e_name_source *sources,
                              char *buf,
                              size_t buf_size,
                              e_addr_name_source *name_source_out,
                              const void **extra_data_out) {
    const s_trusted_name *tname =
        get_trusted_name(type_count, types, source_count, sources, &chain_id, addr);
    if (tname != NULL) {
        strlcpy(buf, tname->name, buf_size);
        if (name_source_out != NULL) *name_source_out = ADDR_NAME_FROM_TRUSTED_NAME;
        if (extra_data_out != NULL) *extra_data_out = tname;
    } else {
        getEthDisplayableAddress(addr, buf, buf_size, chain_id);
        if (name_source_out != NULL) *name_source_out = ADDR_NAME_FROM_RAW;
        if (extra_data_out != NULL) *extra_data_out = NULL;
    }
    return true;
}

/**
 * @brief Mock implementation of get_public_key
 */
uint16_t get_public_key(uint8_t *out, uint8_t outLength) {
    uint16_t status = g_get_public_key_ret;
    if (status == SWO_SUCCESS) {
        // Fill with dummy wallet address
        // clang-format off
        uint8_t wallet_addr[ADDRESS_LENGTH] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11,
                                               0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                               0x99, 0x00, 0x11, 0x22, 0x33, 0x44};
        // clang-format on

        memcpy(out, wallet_addr, ADDRESS_LENGTH);
    }
    return status;
}

static size_t g_network_out_size = 0;

static bool network_from_chain_id_stub(char *out,
                                       size_t out_size,
                                       uint64_t chain_id,
                                       int cmock_num_calls) {
    (void) chain_id;
    (void) cmock_num_calls;
    g_network_out_size = out_size;
    const char *name = g_get_network_as_string_from_chain_id_ret;
    if (name == NULL) return false;
    strlcpy(out, name, out_size);
    return true;
}

// =============================================================================
// Test cases
// =============================================================================

/**
 * @brief Test PARAM_VISIBILITY_ALWAYS with trusted name found
 */
void test_trusted_name_always_with_trusted_name(void) {
    // clang-format off
    // Create an address: 0x742d35Cc6634C0532925a3b844Bc9e7595f0bEb
    CREATE_ADDRESS_PARAM(param,
        0x07, 0x42, 0xd3, 0x5C, 0xc6, 0x63, 0x4C, 0x05, 0x32, 0x92,
        0x5a, 0x3b, 0x84, 0x4B, 0xc9, 0xe7, 0x59, 0x5f, 0x0b, 0xEb
    );
    // clang-format on

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_trusted_name = param,
                     .name = "To"};

    // Mock trusted name lookup
    g_get_current_tx_chain_id_ret = 1;

    static s_trusted_name trusted = {.name = "Vitalik.eth"};
    g_get_trusted_name_ret = &trusted;

    // Should add trusted name to field table
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_trusted_name(&field));
}

/**
 * @brief Test PARAM_VISIBILITY_ALWAYS without trusted name (display raw address)
 */
void test_trusted_name_always_without_trusted_name(void) {
    // clang-format off
    CREATE_ADDRESS_PARAM(param,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_trusted_name = param,
                     .name = "From"};

    // Mock: no trusted name found
    g_get_current_tx_chain_id_ret = 1;
    g_get_trusted_name_ret = NULL;

    // Should display raw address (full checksummed address from real getEthDisplayableAddress)
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_trusted_name(&field));
}

/**
 * @brief Test PARAM_VISIBILITY_MUST_BE with matching constraint (should NOT display)
 */
void test_trusted_name_must_be_valid(void) {
    // clang-format off
    uint8_t expected_addr[ADDRESS_LENGTH] = {0x07, 0x42, 0xd3, 0x5C, 0xc6, 0x63, 0x4C,
                                             0x05, 0x32, 0x92, 0x5a, 0x3b, 0x84, 0x4B,
                                             0xc9, 0xe7, 0x59, 0x5f, 0x0b, 0xEb};

    CREATE_ADDRESS_PARAM(param,
        0x07, 0x42, 0xd3, 0x5C, 0xc6, 0x63, 0x4C, 0x05, 0x32, 0x92,
        0x5a, 0x3b, 0x84, 0x4B, 0xc9, 0xe7, 0x59, 0x5f, 0x0b, 0xEb
    );
    // clang-format on

    // Add constraint: address MUST BE this specific address
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = ADDRESS_LENGTH;
    constraint->value = malloc(ADDRESS_LENGTH);
    memcpy(constraint->value, expected_addr, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_trusted_name = param,
                     .name = "Recipient"};

    // Mock
    g_get_current_tx_chain_id_ret = 1;

    static s_trusted_name trusted = {.name = "Treasury.eth"};
    g_get_trusted_name_ret = &trusted;

    // Should NOT call add_to_field_table (constraint matched, field hidden)

    TEST_ASSERT_TRUE(format_param_trusted_name(&field));

    // Cleanup
    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test PARAM_VISIBILITY_MUST_BE with non-matching constraint (should reject TX)
 */
void test_trusted_name_must_be_invalid(void) {
    // clang-format off
    uint8_t expected_addr[ADDRESS_LENGTH] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    CREATE_ADDRESS_PARAM(param,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    // Add constraint: address MUST BE 0xFFFF...
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = ADDRESS_LENGTH;
    constraint->value = malloc(ADDRESS_LENGTH);
    memcpy(constraint->value, expected_addr, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_trusted_name = param,
                     .name = "Recipient"};

    // Mock
    g_get_current_tx_chain_id_ret = 1;

    static s_trusted_name trusted = {.name = "Unknown.eth"};
    g_get_trusted_name_ret = &trusted;

    // Should FAIL because address doesn't match MUST_BE constraint
    TEST_ASSERT_FALSE(format_param_trusted_name(&field));

    // Cleanup
    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test PARAM_VISIBILITY_IF_NOT_IN with address in constraint list (should NOT display)
 */
void test_trusted_name_if_not_in_match(void) {
    // clang-format off
    uint8_t blacklisted_addr[ADDRESS_LENGTH] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                                0x88, 0x99, 0x00, 0xaa, 0xbb, 0xcc, 0xdd,
                                                0xee, 0xff, 0x11, 0x22, 0x33, 0x44};

    CREATE_ADDRESS_PARAM(param,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    // Add constraint: display IF_NOT_IN this list (i.e., if address is in list, don't display)
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = ADDRESS_LENGTH;
    constraint->value = malloc(ADDRESS_LENGTH);
    memcpy(constraint->value, blacklisted_addr, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_trusted_name = param,
                     .name = "Spender"};

    // Mock
    g_get_current_tx_chain_id_ret = 1;

    static s_trusted_name trusted = {.name = "Blacklisted.eth"};
    g_get_trusted_name_ret = &trusted;

    // Should NOT call add_to_field_table (address is in IF_NOT_IN list)

    TEST_ASSERT_TRUE(format_param_trusted_name(&field));

    // Cleanup
    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test PARAM_VISIBILITY_IF_NOT_IN with address NOT in constraint list (should display)
 */
void test_trusted_name_if_not_in_no_match(void) {
    // clang-format off
    uint8_t blacklisted_addr[ADDRESS_LENGTH] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    CREATE_ADDRESS_PARAM(param,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    // Add constraint: different address in blacklist
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = ADDRESS_LENGTH;
    constraint->value = malloc(ADDRESS_LENGTH);
    memcpy(constraint->value, blacklisted_addr, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_trusted_name = param,
                     .name = "Spender"};

    // Mock
    g_get_current_tx_chain_id_ret = 1;
    g_get_trusted_name_ret = NULL;

    // Should display raw address (not in blacklist, full checksummed address)
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_trusted_name(&field));

    // Cleanup
    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test with sender_addr replacement (wallet address)
 */
void test_trusted_name_sender_addr_replacement(void) {
    // clang-format off
    uint8_t sender_wallet[ADDRESS_LENGTH] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11,
                                             0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                             0x99, 0x00, 0x11, 0x22, 0x33, 0x44};

    CREATE_ADDRESS_PARAM(param,
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    // Add sender address to param
    param.sender_addr_count = 1;
    memcpy(param.sender_addr[0], sender_wallet, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_trusted_name = param,
                     .name = "From"};

    // Mock
    g_get_current_tx_chain_id_ret = 1;

    // Should replace with wallet address
    g_get_public_key_ret = SWO_SUCCESS;

    // Mock trusted name lookup with replaced address
    g_get_trusted_name_ret = NULL;
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_trusted_name(&field));
}

/**
 * @brief Test with chain_id = 0 (should fail)
 */
void test_trusted_name_chain_id_zero(void) {
    // clang-format off
    CREATE_ADDRESS_PARAM(param,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_trusted_name = param,
                     .name = "To"};

    // Mock: chain_id = 0 is invalid
    g_get_current_tx_chain_id_ret = 0;

    TEST_ASSERT_FALSE(format_param_trusted_name(&field));
}

/**
 * @brief Test INTEROPERABLE address with trusted name and known network
 */
void test_trusted_name_interoperable_named(void) {
    // clang-format off
    CREATE_INTEROP_PARAM(param, 0x01,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_trusted_name = param,
                     .name = "To"};

    // No get_current_tx_chain_id call — chain_id comes from EIP-7930 bytes
    static s_trusted_name trusted = {.name = "Vitalik.eth"};
    g_get_trusted_name_ret = &trusted;
    g_get_network_as_string_from_chain_id_ret = "Ethereum";
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_trusted_name(&field));
}

/**
 * @brief Test INTEROPERABLE address without trusted name (raw address) with known network
 */
void test_trusted_name_interoperable_raw(void) {
    // clang-format off
    CREATE_INTEROP_PARAM(param, 0x01,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_trusted_name = param,
                     .name = "To"};
    g_get_trusted_name_ret = NULL;
    g_get_network_as_string_from_chain_id_ret = "Ethereum";
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_trusted_name(&field));
}

/**
 * @brief Test INTEROPERABLE address on an unknown network with a 20-digit chain ID
 */
void test_trusted_name_interoperable_unknown_network(void) {
    // clang-format off
    CREATE_INTEROP_PARAM(param, 0x01,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_trusted_name = param,
                     .name = "To"};
    g_get_trusted_name_ret = NULL;
    // unknown network: the chain ID is used as the network string
    g_get_network_as_string_from_chain_id_ret = "18446744073709551615";
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_trusted_name(&field));
    TEST_ASSERT_GREATER_THAN(strlen("18446744073709551615"), g_network_out_size);
}

/**
 * @brief Test INTEROPERABLE address whose network string cannot be produced
 */
void test_trusted_name_interoperable_network_failure_rejected(void) {
    // clang-format off
    CREATE_INTEROP_PARAM(param, 0x01,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_trusted_name = param,
                     .name = "To"};
    g_get_trusted_name_ret = NULL;
    g_get_network_as_string_from_chain_id_ret = NULL;
    g_add_to_field_table_ret = true;

    TEST_ASSERT_FALSE(format_param_trusted_name(&field));
}

// =============================================================================
// TLV tag-handler tests for handle_param_trusted_name_struct.
//
// Each handle_X tag dispatch is exercised by feeding a hand-crafted TLV
// buffer through the public entry point. VALUE (0x01) passes an empty
// inner TLV — the value sub-parser is real but accepts empty input as a
// no-op.
//
// Tags here are all < 0x80 so short-form encoding is used (no DER long
// form).
// =============================================================================

#include "gtp_param_trusted_name.h"

void test_handle_tn_struct_all_tags_ok(void) {
    uint8_t buf_bytes[] = {
        0x00, 0x01, 0x01,             // VERSION = 1
        0x01, 0x00,                   // VALUE = empty (inner parser sees 0 tags)
        0x02, 0x01, TN_TYPE_ACCOUNT,  // TYPES = [ACCOUNT]
        0x03, 0x01, TN_SOURCE_CAL,    // SOURCES = [CAL]
        0x04, 0x04, 0xDE,
        0xAD, 0xBE, 0xEF,                // SENDER_ADDR (1st, short)
        0x05, 0x01, TNVT_INTEROPERABLE,  // VALUE_TYPE
    };
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_trusted_name param;
    memset(&param, 0, sizeof(param));
    s_param_trusted_name_context ctx = {.param = &param};
    TEST_ASSERT_TRUE(handle_param_trusted_name_struct(&buf, &ctx));
    TEST_ASSERT_EQUAL(param.version, 1);
    TEST_ASSERT_EQUAL(param.type_count, 1);
    TEST_ASSERT_EQUAL(param.types[0], TN_TYPE_ACCOUNT);
    TEST_ASSERT_EQUAL(param.source_count, 1);
    TEST_ASSERT_EQUAL(param.sources[0], TN_SOURCE_CAL);
    TEST_ASSERT_EQUAL(param.sender_addr_count, 1);
    TEST_ASSERT_EQUAL(param.value_type, TNVT_INTEROPERABLE);
}

void test_handle_tn_struct_types_oversize_rejected(void) {
    // sizeof(types[]) — e_name_type is int (4 bytes on this target) so
    // the field is TN_TYPE_COUNT*4 bytes wide. Overflow it by passing
    // one byte more than the byte width.
    const size_t types_byte_width = sizeof(((s_param_trusted_name *) 0)->types);
    uint8_t buf_bytes[3 + 2 + 64];
    buf_bytes[0] = 0x00;
    buf_bytes[1] = 0x01;
    buf_bytes[2] = 0x01;
    buf_bytes[3] = 0x02;  // TYPES
    buf_bytes[4] = (uint8_t) (types_byte_width + 1);
    memset(buf_bytes + 5, 0, types_byte_width + 1);
    buffer_t buf = {.ptr = buf_bytes, .size = 5 + types_byte_width + 1, .offset = 0};

    s_param_trusted_name param;
    memset(&param, 0, sizeof(param));
    s_param_trusted_name_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_trusted_name_struct(&buf, &ctx));
}

void test_handle_tn_struct_sources_oversize_rejected(void) {
    const size_t sources_byte_width = sizeof(((s_param_trusted_name *) 0)->sources);
    uint8_t buf_bytes[3 + 2 + 64];
    buf_bytes[0] = 0x00;
    buf_bytes[1] = 0x01;
    buf_bytes[2] = 0x01;
    buf_bytes[3] = 0x03;  // SOURCES
    buf_bytes[4] = (uint8_t) (sources_byte_width + 1);
    memset(buf_bytes + 5, 0, sources_byte_width + 1);
    buffer_t buf = {.ptr = buf_bytes, .size = 5 + sources_byte_width + 1, .offset = 0};

    s_param_trusted_name param;
    memset(&param, 0, sizeof(param));
    s_param_trusted_name_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_trusted_name_struct(&buf, &ctx));
}

void test_handle_tn_struct_sender_addr_oversize_rejected(void) {
    // The size guard is `value.size > sizeof(param->sender_addr)`, i.e.
    // > MAX_SENDER_ADDRS * ADDRESS_LENGTH (=60). Overflow by one byte.
    const size_t total = MAX_SENDER_ADDRS * ADDRESS_LENGTH + 1;
    uint8_t buf_bytes[3 + 2 + 100];
    buf_bytes[0] = 0x00;
    buf_bytes[1] = 0x01;
    buf_bytes[2] = 0x01;
    buf_bytes[3] = 0x04;  // SENDER_ADDR
    buf_bytes[4] = (uint8_t) total;
    memset(buf_bytes + 5, 0xAA, total);
    buffer_t buf = {.ptr = buf_bytes, .size = 5 + total, .offset = 0};

    s_param_trusted_name param;
    memset(&param, 0, sizeof(param));
    s_param_trusted_name_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_trusted_name_struct(&buf, &ctx));
}

void test_handle_tn_struct_sender_addr_overflow_capacity_rejected(void) {
    // VERSION + MAX_SENDER_ADDRS+1 sender entries → the 4th overflows.
    uint8_t buf_bytes[3 + (MAX_SENDER_ADDRS + 1) * 3];
    buf_bytes[0] = 0x00;
    buf_bytes[1] = 0x01;
    buf_bytes[2] = 0x01;
    for (int i = 0; i < MAX_SENDER_ADDRS + 1; i++) {
        buf_bytes[3 + i * 3 + 0] = 0x04;  // SENDER_ADDR
        buf_bytes[3 + i * 3 + 1] = 0x01;
        buf_bytes[3 + i * 3 + 2] = 0x20 + i;
    }
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_trusted_name param;
    memset(&param, 0, sizeof(param));
    s_param_trusted_name_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_trusted_name_struct(&buf, &ctx));
}

void test_handle_tn_struct_value_type_out_of_range_rejected(void) {
    // VERSION + VALUE_TYPE = 0xFF (outside [STANDARD, INTEROPERABLE])
    uint8_t buf_bytes[] = {
        0x00,
        0x01,
        0x01,
        0x05,
        0x01,
        0xFF,
    };
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_trusted_name param;
    memset(&param, 0, sizeof(param));
    s_param_trusted_name_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_trusted_name_struct(&buf, &ctx));
}

// =============================================================================
// Test runner
// =============================================================================

void setUp(void) {
    Mocktrusted_name_Init();
    Mocknetwork_Init();
    g_get_trusted_name_ret = NULL;
    g_get_network_as_string_from_chain_id_ret = NULL;
    get_trusted_name_StubWithCallback(trusted_name_stub);
    get_network_as_string_from_chain_id_StubWithCallback(network_from_chain_id_stub);
}
void tearDown(void) {
    Mocktrusted_name_Verify();
    Mocktrusted_name_Destroy();
    Mocknetwork_Verify();
    Mocknetwork_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_trusted_name_always_with_trusted_name);
    RUN_TEST(test_trusted_name_always_without_trusted_name);
    RUN_TEST(test_trusted_name_must_be_valid);
    RUN_TEST(test_trusted_name_must_be_invalid);
    RUN_TEST(test_trusted_name_if_not_in_match);
    RUN_TEST(test_trusted_name_if_not_in_no_match);
    RUN_TEST(test_trusted_name_sender_addr_replacement);
    RUN_TEST(test_trusted_name_chain_id_zero);
    RUN_TEST(test_trusted_name_interoperable_named);
    RUN_TEST(test_trusted_name_interoperable_raw);
    RUN_TEST(test_trusted_name_interoperable_unknown_network);
    RUN_TEST(test_trusted_name_interoperable_network_failure_rejected);
    RUN_TEST(test_handle_tn_struct_all_tags_ok);
    RUN_TEST(test_handle_tn_struct_types_oversize_rejected);
    RUN_TEST(test_handle_tn_struct_sources_oversize_rejected);
    RUN_TEST(test_handle_tn_struct_sender_addr_oversize_rejected);
    RUN_TEST(test_handle_tn_struct_sender_addr_overflow_capacity_rejected);
    RUN_TEST(test_handle_tn_struct_value_type_out_of_range_rejected);
    return UNITY_END();
}
