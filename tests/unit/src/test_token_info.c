/**
 * @file test_token_info.c
 * @brief Unit tests for the ERC-20 token-info registry at
 *        src/features/provide_erc20_token_information/token_info.c.
 *
 * The registry is the device's cache of (chain_id, contract_address)
 * -> (ticker, decimals) lookups used by every ERC-20 amount renderer
 * and by token_amount / token GCS parameters. Four public entry
 * points:
 *   - set_token_info: insert or update an entry, returning its
 *     position in the list (the same call site can ask for an index
 *     it remembered to refer back to the entry later).
 *   - clear_token_infos: drop all entries.
 *   - get_matching_token_info: strict (chain_id, address) lookup or
 *     NULL.
 *   - get_matching_token_info_or_dummy: lookup-or-create variant —
 *     missing entries are auto-registered with ticker="???" and
 *     decimals=0 so downstream code never has to special-case
 *     "unknown" tokens.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "token_info.h"

// Storage lives in mocks/mock.c so the linker resolves it without
// dragging network.h's nbgl_types.h chain through this lightweight
// target.
extern const char g_unknown_ticker[];

// =============================================================================
// Globals expected by token_info.c (via network.h)
// =============================================================================

// =============================================================================
// Fixtures
// =============================================================================

static void reset(void) {
    clear_token_infos();
}

static const uint8_t g_usdc[ADDRESS_LENGTH] = {
    0xA0, 0xb8, 0x69, 0x91, 0xc6, 0x21, 0x8b, 0x36, 0xc1, 0xd1,
    0x9D, 0x4a, 0x2e, 0x9E, 0xb0, 0xcE, 0x36, 0x06, 0xeB, 0x48,
};
static const uint8_t g_dai[ADDRESS_LENGTH] = {
    0x6B, 0x17, 0x54, 0x74, 0xE8, 0x90, 0x94, 0xC4, 0x4D, 0xa9,
    0x8b, 0x95, 0x4E, 0xed, 0xeA, 0xC4, 0x95, 0x27, 0x1d, 0x0F,
};

// =============================================================================
// set_token_info
// =============================================================================

void test_set_null_info_rejected(void) {
    TEST_ASSERT_EQUAL(set_token_info(NULL), -1);
}

void test_set_first_entry_returns_index_zero(void) {
    s_token_info info = {.chain_id = 1, .decimals = 6};
    memcpy(info.address, g_usdc, ADDRESS_LENGTH);
    strlcpy(info.ticker, "USDC", sizeof(info.ticker));

    TEST_ASSERT_EQUAL(set_token_info(&info), 0);
    const s_token_info *fetched = get_matching_token_info(&info.chain_id, info.address);
    TEST_ASSERT_NOT_NULL(fetched);
    TEST_ASSERT_EQUAL_STRING(fetched->ticker, "USDC");
    TEST_ASSERT_EQUAL(fetched->decimals, 6);
}

void test_set_second_distinct_entry_returns_index_one(void) {
    s_token_info a = {.chain_id = 1, .decimals = 6};
    memcpy(a.address, g_usdc, ADDRESS_LENGTH);
    strlcpy(a.ticker, "USDC", sizeof(a.ticker));
    s_token_info b = {.chain_id = 1, .decimals = 18};
    memcpy(b.address, g_dai, ADDRESS_LENGTH);
    strlcpy(b.ticker, "DAI", sizeof(b.ticker));

    TEST_ASSERT_EQUAL(set_token_info(&a), 0);
    TEST_ASSERT_EQUAL(set_token_info(&b), 1);
}

void test_set_existing_entry_updates_in_place_returns_same_index(void) {
    s_token_info v1 = {.chain_id = 1, .decimals = 6};
    memcpy(v1.address, g_usdc, ADDRESS_LENGTH);
    strlcpy(v1.ticker, "USDC", sizeof(v1.ticker));
    TEST_ASSERT_EQUAL(set_token_info(&v1), 0);

    // Same (chain_id, address) — second call must update the existing
    // entry and return the same index.
    s_token_info v2 = {.chain_id = 1, .decimals = 8};
    memcpy(v2.address, g_usdc, ADDRESS_LENGTH);
    strlcpy(v2.ticker, "usdc", sizeof(v2.ticker));
    TEST_ASSERT_EQUAL(set_token_info(&v2), 0);

    const s_token_info *fetched = get_matching_token_info(&v1.chain_id, v1.address);
    TEST_ASSERT_NOT_NULL(fetched);
    TEST_ASSERT_EQUAL_STRING(fetched->ticker, "usdc");
    TEST_ASSERT_EQUAL(fetched->decimals, 8);
}

void test_set_same_address_different_chain_is_a_new_entry(void) {
    s_token_info eth = {.chain_id = 1, .decimals = 6};
    memcpy(eth.address, g_usdc, ADDRESS_LENGTH);
    strlcpy(eth.ticker, "USDC", sizeof(eth.ticker));
    s_token_info polygon = {.chain_id = 137, .decimals = 6};
    memcpy(polygon.address, g_usdc, ADDRESS_LENGTH);
    strlcpy(polygon.ticker, "USDC.e", sizeof(polygon.ticker));

    TEST_ASSERT_EQUAL(set_token_info(&eth), 0);
    TEST_ASSERT_EQUAL(set_token_info(&polygon), 1);

    const s_token_info *e1 = get_matching_token_info(&eth.chain_id, eth.address);
    const s_token_info *e2 = get_matching_token_info(&polygon.chain_id, polygon.address);
    TEST_ASSERT_EQUAL_STRING(e1->ticker, "USDC");
    TEST_ASSERT_EQUAL_STRING(e2->ticker, "USDC.e");
}

// =============================================================================
// get_matching_token_info
// =============================================================================

void test_get_null_inputs_return_null(void) {
    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_matching_token_info(NULL, g_usdc));
    TEST_ASSERT_NULL(get_matching_token_info(&chain, NULL));
}

void test_get_empty_registry_returns_null(void) {
    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_matching_token_info(&chain, g_usdc));
}

void test_get_unknown_address_returns_null(void) {
    s_token_info info = {.chain_id = 1, .decimals = 6};
    memcpy(info.address, g_usdc, ADDRESS_LENGTH);
    strlcpy(info.ticker, "USDC", sizeof(info.ticker));
    set_token_info(&info);

    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_matching_token_info(&chain, g_dai));
}

void test_get_unknown_chain_returns_null(void) {
    s_token_info info = {.chain_id = 1, .decimals = 6};
    memcpy(info.address, g_usdc, ADDRESS_LENGTH);
    strlcpy(info.ticker, "USDC", sizeof(info.ticker));
    set_token_info(&info);

    uint64_t chain = 137;  // not registered for this address
    TEST_ASSERT_NULL(get_matching_token_info(&chain, g_usdc));
}

// =============================================================================
// get_matching_token_info_or_dummy
// =============================================================================

void test_dummy_null_inputs_return_null(void) {
    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_matching_token_info_or_dummy(NULL, g_usdc));
    TEST_ASSERT_NULL(get_matching_token_info_or_dummy(&chain, NULL));
}

void test_dummy_known_returns_existing(void) {
    s_token_info info = {.chain_id = 1, .decimals = 6};
    memcpy(info.address, g_usdc, ADDRESS_LENGTH);
    strlcpy(info.ticker, "USDC", sizeof(info.ticker));
    set_token_info(&info);

    uint64_t chain = 1;
    const s_token_info *got = get_matching_token_info_or_dummy(&chain, g_usdc);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_STRING(got->ticker, "USDC");
    TEST_ASSERT_EQUAL(got->decimals, 6);
}

void test_dummy_unknown_creates_unknown_marker(void) {
    uint64_t chain = 1;
    const s_token_info *got = get_matching_token_info_or_dummy(&chain, g_dai);
    TEST_ASSERT_NOT_NULL(got);
    // The unknown marker uses g_unknown_ticker ("???") and decimals=0
    // so downstream renderers can fall through with no special case.
    TEST_ASSERT_EQUAL_STRING(got->ticker, g_unknown_ticker);
    TEST_ASSERT_EQUAL(got->decimals, 0);
    TEST_ASSERT_EQUAL(got->chain_id, 1);
    TEST_ASSERT_EQUAL_MEMORY(got->address, g_dai, ADDRESS_LENGTH);

    // Subsequent get_matching_token_info now returns the registered
    // dummy entry (the side effect is persistent).
    TEST_ASSERT_EQUAL_PTR(get_matching_token_info(&chain, g_dai), got);
}

void test_dummy_persists_across_calls(void) {
    // Two consecutive get_or_dummy calls for the same key must return
    // the same registered entry, NOT create two dummies.
    uint64_t chain = 1;
    const s_token_info *a = get_matching_token_info_or_dummy(&chain, g_dai);
    const s_token_info *b = get_matching_token_info_or_dummy(&chain, g_dai);
    TEST_ASSERT_EQUAL_PTR(a, b);
}

// =============================================================================
// clear_token_infos
// =============================================================================

void test_clear_empties_registry(void) {
    s_token_info a = {.chain_id = 1, .decimals = 6};
    memcpy(a.address, g_usdc, ADDRESS_LENGTH);
    strlcpy(a.ticker, "USDC", sizeof(a.ticker));
    s_token_info b = {.chain_id = 1, .decimals = 18};
    memcpy(b.address, g_dai, ADDRESS_LENGTH);
    strlcpy(b.ticker, "DAI", sizeof(b.ticker));
    set_token_info(&a);
    set_token_info(&b);

    clear_token_infos();

    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_matching_token_info(&chain, g_usdc));
    TEST_ASSERT_NULL(get_matching_token_info(&chain, g_dai));
    // After clear, the index counter restarts from 0 — verifies the
    // list was actually freed, not just unlinked.
    TEST_ASSERT_EQUAL(set_token_info(&a), 0);
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
    RUN_TEST(test_set_null_info_rejected);
    RUN_TEST(test_set_first_entry_returns_index_zero);
    RUN_TEST(test_set_second_distinct_entry_returns_index_one);
    RUN_TEST(test_set_existing_entry_updates_in_place_returns_same_index);
    RUN_TEST(test_set_same_address_different_chain_is_a_new_entry);
    RUN_TEST(test_get_null_inputs_return_null);
    RUN_TEST(test_get_empty_registry_returns_null);
    RUN_TEST(test_get_unknown_address_returns_null);
    RUN_TEST(test_get_unknown_chain_returns_null);
    RUN_TEST(test_dummy_null_inputs_return_null);
    RUN_TEST(test_dummy_known_returns_existing);
    RUN_TEST(test_dummy_unknown_creates_unknown_marker);
    RUN_TEST(test_dummy_persists_across_calls);
    RUN_TEST(test_clear_empties_registry);
    return UNITY_END();
}
