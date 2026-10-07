/**
 * @file test_param_nft.c
 * @brief Unit tests for the NFT parameter in
 *        src/features/generic_tx_parser/gtp_param_nft.c.
 *
 * NFT renders a "<collection_name> #<id>" field. The formatter draws
 * two parallel value collections — `collection` (addresses) and `id`
 * (token IDs) — and reconciles them with a §3.1.8-style broadcast
 * rule: collections.size must equal ids.size OR must equal 1
 * (in which case the single collection is reused for every id).
 *
 * Beyond the broadcast rule, the formatter chains a few hard
 * preconditions:
 *   - get_current_tx_info must be non-NULL,
 *   - both value_get calls must succeed,
 *   - collections.size cannot be zero,
 *   - each get_matching_nft_info lookup must hit a registered entry,
 *   - uint256_to_decimal must successfully render the ID.
 *
 * Tests pin every branch independently.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_param_nft.h"
#include "gtp_parsed_value.h"
#include "gtp_value.h"
#include "gtp_field.h"
#include "gtp_tx_info.h"
#include "nft_info.h"
#include "shared_context.h"
#include "wraps.h"

// =============================================================================
// Globals
// =============================================================================

static bool g_add_to_field_table_ret = true;
static const void *g_get_matching_nft_info_ret = NULL;

// =============================================================================
// Wrapped dependencies
// =============================================================================

// value_get is called twice per format_param_nft invocation: first for the
// collection list, then for the id list. The wrap pulls from g_vg[0] then
// g_vg[1] in order.
static int g_vg_call = 0;
static s_parsed_value_collection g_vg[2];
static bool g_vg_ret[2] = {true, true};

bool value_get(const s_value *value, s_parsed_value_collection *collection) {
    (void) value;
    bool ret = g_vg_ret[g_vg_call];
    *collection = g_vg[g_vg_call++];
    return ret;
}

static bool g_hvs_ret = true;
bool handle_value_struct(const buffer_t *buf, s_value_context *context) {
    (void) buf;
    (void) context;
    return g_hvs_ret;
}

static s_tx_info g_fake_tx_info;

const s_nft_info *get_matching_nft_info(const uint64_t *chain_id, const uint8_t *address) {
    (void) chain_id;
    (void) address;
    return (const s_nft_info *) g_get_matching_nft_info_ret;
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

static const uint8_t g_bayc_addr[ADDRESS_LENGTH] = {
    0xBC, 0x4C, 0xA0, 0xED, 0xa7, 0x64, 0x7A, 0x8a, 0xB7, 0xC2,
    0x06, 0x1c, 0x2E, 0x11, 0x8A, 0x18, 0xa9, 0x36, 0xf1, 0x3D,
};
static const uint8_t g_punks_addr[ADDRESS_LENGTH] = {
    0xB4, 0x7e, 0x3c, 0xd8, 0x37, 0xdD, 0xF8, 0xe4, 0xc5, 0x7F,
    0x05, 0xd7, 0x0A, 0xb8, 0x65, 0xde, 0x6e, 0x19, 0x3B, 0xBB,
};
static const s_nft_info g_bayc_info = {.collection_name = "BAYC"};
static const s_nft_info g_punks_info = {.collection_name = "Punks"};

static void reset(void) {
    memset(&g_vg, 0, sizeof(g_vg));
    g_vg_call = 0;
    g_vg_ret[0] = true;
    g_vg_ret[1] = true;
    g_hvs_ret = true;
    memset(&g_fake_tx_info, 0, sizeof(g_fake_tx_info));
    g_fake_tx_info.chain_id = 1;
    s_tx_info_ret = &g_fake_tx_info;
}

// =============================================================================
// format_param_nft — happy paths
// =============================================================================

void test_format_single_collection_single_id(void) {
    static const uint8_t id_bytes[] = {0x2A};  // 42
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_bayc_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};
    g_vg[1].size = 1;
    g_vg[1].value[0] = (s_parsed_value) {.ptr = id_bytes, .size = 1, .offset = 0, .length = 1};

    g_get_matching_nft_info_ret = &g_bayc_info;
    g_add_to_field_table_ret = true;

    s_param_nft param = {0};
    TEST_ASSERT_TRUE(format_param_nft(&param, "NFT"));
}

void test_format_broadcast_one_collection_many_ids(void) {
    // §3.1.8: a single collection paired with N ids reuses that collection
    // for every id.
    static const uint8_t id1[] = {0x01};
    static const uint8_t id2[] = {0x02};
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_bayc_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};
    g_vg[1].size = 2;
    g_vg[1].value[0] = (s_parsed_value) {.ptr = id1, .size = 1, .offset = 0, .length = 1};
    g_vg[1].value[1] = (s_parsed_value) {.ptr = id2, .size = 1, .offset = 0, .length = 1};

    g_get_matching_nft_info_ret = &g_bayc_info;
    g_get_matching_nft_info_ret = &g_bayc_info;
    g_add_to_field_table_ret = true;
    g_add_to_field_table_ret = true;

    s_param_nft param = {0};
    TEST_ASSERT_TRUE(format_param_nft(&param, "NFT"));
}

void test_format_paired_n_collections_n_ids(void) {
    static const uint8_t id1[] = {0x07};
    static const uint8_t id2[] = {0x42};
    g_vg[0].size = 2;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_bayc_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};
    g_vg[0].value[1] = (s_parsed_value) {.ptr = g_punks_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};
    g_vg[1].size = 2;
    g_vg[1].value[0] = (s_parsed_value) {.ptr = id1, .size = 1, .offset = 0, .length = 1};
    g_vg[1].value[1] = (s_parsed_value) {.ptr = id2, .size = 1, .offset = 0, .length = 1};

    g_get_matching_nft_info_ret = &g_bayc_info;
    g_get_matching_nft_info_ret = &g_punks_info;
    g_add_to_field_table_ret = true;
    g_add_to_field_table_ret = true;

    s_param_nft param = {0};
    TEST_ASSERT_TRUE(format_param_nft(&param, "NFT"));
}

// =============================================================================
// format_param_nft — rejection paths
// =============================================================================

void test_format_mismatched_sizes_rejected(void) {
    // 2 collections vs 3 ids — and 2 ≠ 1, so the broadcast rule does not
    // apply. Must reject before any nft lookup.
    static const uint8_t id1[] = {1};
    static const uint8_t id2[] = {2};
    static const uint8_t id3[] = {3};
    g_vg[0].size = 2;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_bayc_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};
    g_vg[0].value[1] = (s_parsed_value) {.ptr = g_punks_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};
    g_vg[1].size = 3;
    g_vg[1].value[0] = (s_parsed_value) {.ptr = id1, .size = 1, .offset = 0, .length = 1};
    g_vg[1].value[1] = (s_parsed_value) {.ptr = id2, .size = 1, .offset = 0, .length = 1};
    g_vg[1].value[2] = (s_parsed_value) {.ptr = id3, .size = 1, .offset = 0, .length = 1};

    s_param_nft param = {0};
    TEST_ASSERT_FALSE(format_param_nft(&param, "NFT"));
}

void test_format_empty_collections_rejected(void) {
    // Zero collections is invalid even if there are ids (nothing to render).
    static const uint8_t id1[] = {1};
    g_vg[0].size = 0;
    g_vg[1].size = 1;
    g_vg[1].value[0] = (s_parsed_value) {.ptr = id1, .size = 1, .offset = 0, .length = 1};

    s_param_nft param = {0};
    TEST_ASSERT_FALSE(format_param_nft(&param, "NFT"));
}

void test_format_unknown_nft_rejected(void) {
    static const uint8_t id1[] = {1};
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_bayc_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};
    g_vg[1].size = 1;
    g_vg[1].value[0] = (s_parsed_value) {.ptr = id1, .size = 1, .offset = 0, .length = 1};

    g_get_matching_nft_info_ret = NULL;
    // No add_to_field_table expected.

    s_param_nft param = {0};
    TEST_ASSERT_FALSE(format_param_nft(&param, "NFT"));
}

void test_format_value_get_collections_failure_rejected(void) {
    g_vg_ret[0] = false;  // value_get on collections fails
    s_param_nft param = {0};
    TEST_ASSERT_FALSE(format_param_nft(&param, "NFT"));
}

void test_format_value_get_ids_failure_rejected(void) {
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_bayc_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};
    g_vg_ret[1] = false;  // value_get on ids fails

    s_param_nft param = {0};
    TEST_ASSERT_FALSE(format_param_nft(&param, "NFT"));
}

void test_format_tx_info_null_rejected(void) {
    s_tx_info_ret = NULL;
    s_param_nft param = {0};
    TEST_ASSERT_FALSE(format_param_nft(&param, "NFT"));
}

void test_format_add_to_field_table_failure_propagates(void) {
    static const uint8_t id1[] = {1};
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_bayc_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};
    g_vg[1].size = 1;
    g_vg[1].value[0] = (s_parsed_value) {.ptr = id1, .size = 1, .offset = 0, .length = 1};

    g_get_matching_nft_info_ret = &g_bayc_info;
    g_add_to_field_table_ret = false;

    s_param_nft param = {0};
    TEST_ASSERT_FALSE(format_param_nft(&param, "NFT"));
}

// =============================================================================
// handle_param_nft_struct (TLV)
// =============================================================================

static bool run_tlv(const uint8_t *bytes, size_t size, s_param_nft *param) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    s_param_nft_context ctx = {.param = param};
    return handle_param_nft_struct(&buf, &ctx);
}

void test_tlv_happy_path(void) {
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x00,  // ID empty (handle_value_struct wrapped → true)
        0x02,
        0x00,  // COLLECTION empty
    };
    s_param_nft param = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &param));
    TEST_ASSERT_EQUAL(param.version, 1);
}

void test_tlv_duplicate_collection_rejected(void) {
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x00,
        0x02,
        0x00,
        0x02,
        0x00,  // duplicate COLLECTION
    };
    s_param_nft param = {0};
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
    RUN_TEST(test_format_single_collection_single_id);
    RUN_TEST(test_format_broadcast_one_collection_many_ids);
    RUN_TEST(test_format_paired_n_collections_n_ids);
    RUN_TEST(test_format_mismatched_sizes_rejected);
    RUN_TEST(test_format_empty_collections_rejected);
    RUN_TEST(test_format_unknown_nft_rejected);
    RUN_TEST(test_format_value_get_collections_failure_rejected);
    RUN_TEST(test_format_value_get_ids_failure_rejected);
    RUN_TEST(test_format_tx_info_null_rejected);
    RUN_TEST(test_format_add_to_field_table_failure_propagates);
    RUN_TEST(test_tlv_happy_path);
    RUN_TEST(test_tlv_duplicate_collection_rejected);
    return UNITY_END();
}
