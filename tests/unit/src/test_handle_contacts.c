/**
 * @file test_handle_contacts.c
 * @brief Unit tests for the Address Book contact cache at
 *        src/features/address_book/handle_contacts.c.
 *
 * Behaviors covered:
 *   - name, identifier and scope edits only update the contacts of the edited
 *     group (gid), never a contact of another group sharing the name or address,
 *   - a scope edit also requires the previous scope to match,
 *   - an identifier edit onto an address that already has a contact drops the
 *     edited entry instead of duplicating that address.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "handle_contacts.h"
#include "app_mem_utils.h"

// =============================================================================
// Fixtures
// =============================================================================

#define CHAIN_ID 1

static const uint8_t g_gid_a[GID_SIZE] = {0xA0};
static const uint8_t g_gid_b[GID_SIZE] = {0xB0};
static const uint8_t g_addr_x[ADDRESS_LENGTH] = {0x11};
static const uint8_t g_addr_y[ADDRESS_LENGTH] = {0x22};
static const uint8_t g_addr_z[ADDRESS_LENGTH] = {0x33};

static s_ab_contact *add_contact(const uint8_t *gid,
                                 const char *name,
                                 const char *scope,
                                 const uint8_t *addr) {
    s_ab_contact *node = NULL;
    TEST_ASSERT_TRUE(APP_MEM_CALLOC((void **) &node, sizeof(*node)));
    node->type = AB_CONTACT_IDENTITY;
    memcpy(node->gid, gid, GID_SIZE);
    strlcpy(node->contact_name, name, sizeof(node->contact_name));
    strlcpy(node->scope, scope, sizeof(node->scope));
    memcpy(node->identifier, addr, ADDRESS_LENGTH);
    node->chain_id = CHAIN_ID;
    ab_contact_list_push(node);
    return node;
}

// =============================================================================
// Tests
// =============================================================================

void test_rename_only_affects_edited_group(void) {
    const s_ab_contact *alice_a = add_contact(g_gid_a, "Alice", "Main", g_addr_x);
    const s_ab_contact *alice_b = add_contact(g_gid_b, "Alice", "Main", g_addr_y);

    update_contact_name(g_gid_b, "Alice", "Bob");

    TEST_ASSERT_EQUAL_STRING("Alice", alice_a->contact_name);
    TEST_ASSERT_EQUAL_STRING("Bob", alice_b->contact_name);
}

void test_identifier_edit_of_another_group_ignored(void) {
    const s_ab_contact *alice = add_contact(g_gid_a, "Alice", "Main", g_addr_x);

    // Group B registered the same address, but its contact is not in the cache
    update_contact_identifier(g_gid_b, g_addr_x, CHAIN_ID, g_addr_z);

    TEST_ASSERT_EQUAL_MEMORY(g_addr_x, alice->identifier, ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_PTR(alice, get_address_book_contact(CHAIN_ID, g_addr_x));
    TEST_ASSERT_NULL(get_address_book_contact(CHAIN_ID, g_addr_z));
}

void test_identifier_edit_of_same_group_applied(void) {
    const s_ab_contact *alice = add_contact(g_gid_a, "Alice", "Main", g_addr_x);

    update_contact_identifier(g_gid_a, g_addr_x, CHAIN_ID, g_addr_z);

    TEST_ASSERT_EQUAL_MEMORY(g_addr_z, alice->identifier, ADDRESS_LENGTH);
}

void test_scope_edit_of_another_group_ignored(void) {
    const s_ab_contact *alice = add_contact(g_gid_a, "Alice", "Main", g_addr_x);

    update_contact_scope(g_gid_b, g_addr_x, CHAIN_ID, "Main", "Savings");

    TEST_ASSERT_EQUAL_STRING("Main", alice->scope);
}

void test_scope_edit_with_wrong_old_scope_ignored(void) {
    const s_ab_contact *alice = add_contact(g_gid_a, "Alice", "Main", g_addr_x);

    update_contact_scope(g_gid_a, g_addr_x, CHAIN_ID, "Other", "Savings");

    TEST_ASSERT_EQUAL_STRING("Main", alice->scope);
}

void test_scope_edit_of_same_group_applied(void) {
    const s_ab_contact *alice = add_contact(g_gid_a, "Alice", "Main", g_addr_x);

    update_contact_scope(g_gid_a, g_addr_x, CHAIN_ID, "Main", "Savings");

    TEST_ASSERT_EQUAL_STRING("Savings", alice->scope);
}

void test_identifier_edit_onto_cached_address_drops_entry(void) {
    add_contact(g_gid_a, "Alice", "Main", g_addr_x);
    const s_ab_contact *bob = add_contact(g_gid_b, "Bob", "Main", g_addr_y);

    update_contact_identifier(g_gid_a, g_addr_x, CHAIN_ID, g_addr_y);

    // The contact already approved for the new address keeps it, alone
    TEST_ASSERT_EQUAL_PTR(bob, get_address_book_contact(CHAIN_ID, g_addr_y));
    TEST_ASSERT_NULL(get_address_book_contact(CHAIN_ID, g_addr_x));
    TEST_ASSERT_NULL(((const flist_node_t *) bob)->next);
}

void test_identifier_edit_onto_same_address_kept(void) {
    const s_ab_contact *alice = add_contact(g_gid_a, "Alice", "Main", g_addr_x);

    update_contact_identifier(g_gid_a, g_addr_x, CHAIN_ID, g_addr_x);

    TEST_ASSERT_EQUAL_PTR(alice, get_address_book_contact(CHAIN_ID, g_addr_x));
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
}

void tearDown(void) {
    address_book_contact_cleanup();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rename_only_affects_edited_group);
    RUN_TEST(test_identifier_edit_of_another_group_ignored);
    RUN_TEST(test_identifier_edit_of_same_group_applied);
    RUN_TEST(test_identifier_edit_onto_cached_address_drops_entry);
    RUN_TEST(test_identifier_edit_onto_same_address_kept);
    RUN_TEST(test_scope_edit_of_another_group_ignored);
    RUN_TEST(test_scope_edit_with_wrong_old_scope_ignored);
    RUN_TEST(test_scope_edit_of_same_group_applied);
    return UNITY_END();
}
