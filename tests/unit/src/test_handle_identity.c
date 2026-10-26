/**
 * @file test_handle_identity.c
 * @brief Unit tests for the Register Identity and Edit Identifier reviews at
 *        src/features/address_book/handle_identity.c.
 *
 * Behaviors covered:
 *   - get_register_identity_tagValue() allocates the address and network
 *     display buffers once, however many times NBGL refreshes the pairs,
 *   - address_book_review_cleanup() releases everything the review allocated,
 *   - a new review starts from an empty context, whatever a previous flow left,
 *   - get_edit_identifier_tagValue() shows both addresses and the network.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "address_book_entrypoints.h"
#include "address_book_ctx.h"
#include "chain_config.h"
#include "common_utils.h"
#include "handle_contacts.h"
#include "shared_context.h"

// =============================================================================
// Allocator instrumentation (-Wl,--wrap on the app allocator)
// =============================================================================

static int g_live_allocs = 0;

bool __real_mem_utils_calloc(void **buffer,
                             size_t size,
                             bool permanent,
                             const char *file,
                             int line);
void __real_mem_utils_free_and_null(void **buffer, const char *file, int line);

bool __wrap_mem_utils_calloc(void **buffer,
                             size_t size,
                             bool permanent,
                             const char *file,
                             int line) {
    bool ret = __real_mem_utils_calloc(buffer, size, permanent, file, line);
    if (ret && (*buffer != NULL)) {
        g_live_allocs++;
    }
    return ret;
}

void __wrap_mem_utils_free_and_null(void **buffer, const char *file, int line) {
    if (*buffer != NULL) {
        g_live_allocs--;
    }
    __real_mem_utils_free_and_null(buffer, file, line);
}

// =============================================================================
// Collaborators
// =============================================================================

bool getEthAddressStringFromBinary(const uint8_t *address,
                                   char out[static(ADDRESS_LENGTH * 2) + 1],
                                   uint64_t chainId) {
    (void) address;
    (void) chainId;
    memset(out, '0', ADDRESS_LENGTH * 2);
    out[ADDRESS_LENGTH * 2] = '\0';
    return true;
}

bool get_network_as_string_from_chain_id(char *out, size_t out_size, uint64_t chain_id) {
    (void) chain_id;
    strlcpy(out, "Ethereum", out_size);
    return true;
}

void ui_idle(void) {
}

// Contact cache, unreached by the register review
void ab_contact_list_push(s_ab_contact *node) {
    (void) node;
}
const s_ab_contact *get_address_book_contact(uint64_t chain_id, const uint8_t *addr) {
    (void) chain_id;
    (void) addr;
    return NULL;
}
void update_contact_name(const uint8_t *gid, const char *old_name, const char *new_name) {
    (void) gid;
    (void) old_name;
    (void) new_name;
}
void update_contact_identifier(const uint8_t *gid,
                               const uint8_t *old_addr,
                               uint64_t chain_id,
                               const uint8_t *new_addr) {
    (void) gid;
    (void) old_addr;
    (void) chain_id;
    (void) new_addr;
}
void update_contact_scope(const uint8_t *gid,
                          const uint8_t *addr,
                          uint64_t chain_id,
                          const char *old_scope,
                          const char *new_scope) {
    (void) gid;
    (void) addr;
    (void) chain_id;
    (void) old_scope;
    (void) new_scope;
}

// =============================================================================
// Fixtures
// =============================================================================

static identity_t g_identity;

static void fill_identity(void) {
    memset(&g_identity, 0, sizeof(g_identity));
    strlcpy(g_identity.contact_name, "Alice", sizeof(g_identity.contact_name));
    strlcpy(g_identity.scope, "Main", sizeof(g_identity.scope));
    memset(g_identity.identifier, 0x11, ADDRESS_LENGTH);
    g_identity.identifier_len = ADDRESS_LENGTH;
    g_identity.blockchain_family = FAMILY_ETHEREUM;
    g_identity.chain_id = ETHEREUM_MAINNET_CHAINID;
}

// =============================================================================
// Tests
// =============================================================================

void test_register_review_pairs_allocated_once(void) {
    int live = g_live_allocs;
    TEST_ASSERT_TRUE(handle_check_register_identity(&g_identity));
    int after_check = g_live_allocs;

    for (int refresh = 0; refresh < 5; ++refresh) {
        for (uint8_t idx = 0; idx < 4; ++idx) {
            TEST_ASSERT_NOT_NULL(get_register_identity_tagValue(idx));
        }
    }
    // One address and one network buffer, whatever the number of refreshes
    TEST_ASSERT_EQUAL_INT(after_check + 2, g_live_allocs);

    nbgl_contentTagValue_t *pair = get_register_identity_tagValue(2);
    TEST_ASSERT_EQUAL_STRING("Address", pair->item);
    TEST_ASSERT_EQUAL_STRING("0x1111111111111111111111111111111111111111", pair->value);
    pair = get_register_identity_tagValue(3);
    TEST_ASSERT_EQUAL_STRING("Network", pair->item);
    TEST_ASSERT_EQUAL_STRING("Ethereum", pair->value);

    address_book_review_cleanup();
    TEST_ASSERT_EQUAL_INT(live, g_live_allocs);
}

void test_register_review_starts_from_empty_context(void) {
    int live = g_live_allocs;
    // Leftover of a previous review that was never cleaned up
    TEST_ASSERT_TRUE(handle_check_register_identity(&g_identity));
    TEST_ASSERT_NOT_NULL(get_register_identity_tagValue(2));

    TEST_ASSERT_TRUE(handle_check_register_identity(&g_identity));
    TEST_ASSERT_NOT_NULL(get_register_identity_tagValue(2));
    TEST_ASSERT_NOT_NULL(get_register_identity_tagValue(3));

    address_book_review_cleanup();
    TEST_ASSERT_EQUAL_INT(live, g_live_allocs);
}

void test_edit_identifier_review_shows_network(void) {
    int live = g_live_allocs;
    edit_identifier_t edit;
    memset(&edit, 0, sizeof(edit));
    edit.identity = g_identity;
    memset(edit.old_identifier, 0x22, ADDRESS_LENGTH);
    edit.old_identifier_len = ADDRESS_LENGTH;
    TEST_ASSERT_TRUE(handle_check_edit_identifier(&edit));

    nbgl_contentTagValue_t *pair = get_edit_identifier_tagValue(2);
    TEST_ASSERT_EQUAL_STRING("Old address", pair->item);
    TEST_ASSERT_EQUAL_STRING("0x2222222222222222222222222222222222222222", pair->value);
    pair = get_edit_identifier_tagValue(3);
    TEST_ASSERT_EQUAL_STRING("New address", pair->item);
    TEST_ASSERT_EQUAL_STRING("0x1111111111111111111111111111111111111111", pair->value);
    pair = get_edit_identifier_tagValue(4);
    TEST_ASSERT_EQUAL_STRING("Network", pair->item);
    TEST_ASSERT_EQUAL_STRING("Ethereum", pair->value);
    TEST_ASSERT_NULL(get_edit_identifier_tagValue(5));

    address_book_review_cleanup();
    TEST_ASSERT_EQUAL_INT(live, g_live_allocs);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    fill_identity();
}

void tearDown(void) {
    address_book_review_cleanup();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_register_review_pairs_allocated_once);
    RUN_TEST(test_register_review_starts_from_empty_context);
    RUN_TEST(test_edit_identifier_review_shows_network);
    return UNITY_END();
}
