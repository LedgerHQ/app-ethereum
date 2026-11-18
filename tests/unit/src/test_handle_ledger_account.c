/**
 * @file test_handle_ledger_account.c
 * @brief Unit tests for the Provide Ledger Account contact handler at
 *        src/features/address_book/handle_ledger_account.c.
 *
 * Behaviors covered:
 *   - a Ledger Account whose derived address already has a cached contact,
 *     whatever its type, is not cached again: replaying the same proof cannot
 *     grow the contact cache.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "address_book_ctx.h"
#include "address_book_entrypoints.h"
#include "handle_contacts.h"
#include "app_mem_utils.h"
#include "chain_config.h"
#include "nbgl_types.h"
#include "get_public_key.h"

// =============================================================================
// Collaborators
// =============================================================================

static const uint8_t g_derived_addr[ADDRESS_LENGTH] = {0x5A, 0x5A, 0x5A};

cx_err_t bip32_derive_with_seed_get_pubkey_256(unsigned int derivation_mode,
                                               cx_curve_t curve,
                                               const uint32_t *path,
                                               size_t path_len,
                                               uint8_t raw_pubkey[static 65],
                                               uint8_t *chain_code,
                                               cx_md_t hashID,
                                               unsigned char *seed,
                                               size_t seed_len) {
    (void) derivation_mode;
    (void) curve;
    (void) path;
    (void) path_len;
    (void) chain_code;
    (void) hashID;
    (void) seed;
    (void) seed_len;
    memset(raw_pubkey, 0, 65);
    return CX_OK;
}

void getEthAddressFromRawKey(const uint8_t raw_pubkey[static 65],
                             uint8_t out[static ADDRESS_LENGTH]) {
    (void) raw_pubkey;
    memcpy(out, g_derived_addr, ADDRESS_LENGTH);
}

// Review UI, unreached by the provide handler
const nbgl_icon_details_t C_Info_40px;
const nbgl_icon_details_t *get_app_icon(bool caller_icon) {
    (void) caller_icon;
    return NULL;
}
const nbgl_icon_details_t *get_network_icon_from_chain_id(const uint64_t *chain_id) {
    (void) chain_id;
    return NULL;
}
bool get_displayable_address(const bip32_path_t *bip32,
                             char *out,
                             size_t out_size,
                             uint64_t chain_id) {
    (void) bip32;
    (void) out;
    (void) out_size;
    (void) chain_id;
    return true;
}
void nbgl_useCaseAdvancedChoiceWithDetails(const struct nbgl_icon_details_s *icon,
                                           const struct nbgl_icon_details_s *topRightIcon,
                                           const char *message,
                                           const char *subMessage,
                                           const char *smallText,
                                           const char *confirmText,
                                           const char *cancelText,
                                           nbgl_warningDetails_t *details,
                                           nbgl_choiceCallback_t callback) {
    (void) icon;
    (void) topRightIcon;
    (void) message;
    (void) subMessage;
    (void) smallText;
    (void) confirmText;
    (void) cancelText;
    (void) details;
    (void) callback;
}
void ui_idle(void) {
}
void ui_pairs_cleanup(void) {
}

// =============================================================================
// Fixtures
// =============================================================================

static ledger_account_t g_account;

static size_t cached_contacts(void) {
    size_t count = 0;
    for (const flist_node_t *node =
             (const flist_node_t *) get_address_book_contact(ETHEREUM_MAINNET_CHAINID,
                                                             g_derived_addr);
         node != NULL;
         node = node->next) {
        count++;
    }
    return count;
}

// =============================================================================
// Tests
// =============================================================================

void test_replayed_ledger_account_cached_once(void) {
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_TRUE(handle_provide_ledger_account(&g_account));
    }
    TEST_ASSERT_EQUAL(cached_contacts(), 1);
}

void test_ledger_account_behind_identity_not_cached(void) {
    s_ab_contact *identity = NULL;
    TEST_ASSERT_TRUE(APP_MEM_CALLOC((void **) &identity, sizeof(*identity)));
    identity->type = AB_CONTACT_IDENTITY;
    strlcpy(identity->contact_name, "Alice", sizeof(identity->contact_name));
    memcpy(identity->identifier, g_derived_addr, ADDRESS_LENGTH);
    identity->chain_id = ETHEREUM_MAINNET_CHAINID;
    ab_contact_list_push(identity);

    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_TRUE(handle_provide_ledger_account(&g_account));
    }
    TEST_ASSERT_EQUAL(cached_contacts(), 1);
    TEST_ASSERT_EQUAL_PTR(identity,
                          get_address_book_contact(ETHEREUM_MAINNET_CHAINID, g_derived_addr));
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    memset(&g_account, 0, sizeof(g_account));
    strlcpy(g_account.account_name, "Main account", sizeof(g_account.account_name));
    g_account.bip32_path.length = 5;
    g_account.chain_id = ETHEREUM_MAINNET_CHAINID;
    g_account.blockchain_family = FAMILY_ETHEREUM;
}

void tearDown(void) {
    address_book_contact_cleanup();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_replayed_ledger_account_cached_once);
    RUN_TEST(test_ledger_account_behind_identity_not_cached);
    return UNITY_END();
}
