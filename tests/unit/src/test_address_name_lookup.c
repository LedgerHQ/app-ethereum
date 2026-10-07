/**
 * @file test_address_name_lookup.c
 * @brief Unit tests for get_address_display_name (Address Book→Trusted Name→RAW resolution)
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "address_name_lookup.h"
#include "shared_context.h"

#include "os.h"

#include "Mocktrusted_name.h"
#include "Mockhandle_contacts.h"

strings_t strings;

static chain_config_t chainConfig_storage = {.ticker = "ETH", .chain_id = 1, .coin_type = 60};
const chain_config_t *g_chain_config = &chainConfig_storage;

// =============================================================================
// Test helpers
// =============================================================================

// clang-format off
static const uint8_t TEST_ADDR[ADDRESS_LENGTH] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
    0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
};
// clang-format on

static const e_name_type TYPES_ACCOUNT[] = {TN_TYPE_ACCOUNT};
static const e_name_source SOURCES_ENS[] = {TN_SOURCE_ENS};
static const uint64_t CHAIN_ID = 1;

// =============================================================================
// Test cases
// =============================================================================

/**
 * Both Address Book and Trusted Name exist for the same address → Address Book wins, rendering
 * handles Trusted Name separately
 */
void test_conflict_ab_and_tn(void) {
    static s_ab_contact ab = {.contact_name = "Alice"};
    static s_trusted_name tn = {.name = "alice.eth"};
    char buf[64];
    e_addr_name_source name_source;
    const void *extra_data = NULL;

    get_trusted_name_IgnoreAndReturn(&tn);
    get_address_book_contact_IgnoreAndReturn(&ab);

    TEST_ASSERT_TRUE(get_address_display_name(TEST_ADDR,
                                              CHAIN_ID,
                                              1,
                                              TYPES_ACCOUNT,
                                              1,
                                              SOURCES_ENS,
                                              buf,
                                              sizeof(buf),
                                              &name_source,
                                              &extra_data));

    TEST_ASSERT_EQUAL(name_source, ADDR_NAME_FROM_ADDRESS_BOOK);
    TEST_ASSERT_EQUAL_STRING(buf, "Alice");
    TEST_ASSERT_EQUAL_PTR(extra_data, &ab);
}

/**
 * Address Book contact found, no Trusted Name → ADDR_NAME_FROM_ADDRESS_BOOK, contact name in buf
 */
void test_ab_only(void) {
    static s_ab_contact ab = {.contact_name = "Alice"};
    char buf[64];
    e_addr_name_source name_source;
    const void *extra_data = NULL;

    get_trusted_name_IgnoreAndReturn(NULL);
    get_address_book_contact_IgnoreAndReturn(&ab);

    TEST_ASSERT_TRUE(get_address_display_name(TEST_ADDR,
                                              CHAIN_ID,
                                              1,
                                              TYPES_ACCOUNT,
                                              1,
                                              SOURCES_ENS,
                                              buf,
                                              sizeof(buf),
                                              &name_source,
                                              &extra_data));

    TEST_ASSERT_EQUAL(name_source, ADDR_NAME_FROM_ADDRESS_BOOK);
    TEST_ASSERT_EQUAL_STRING(buf, "Alice");
    TEST_ASSERT_EQUAL_PTR(extra_data, &ab);
}

/**
 * No Address Book contact, Trusted Name found → ADDR_NAME_FROM_TRUSTED_NAME, tn name in buf
 */
void test_tn_only(void) {
    static s_trusted_name tn = {.name = "alice.eth"};
    char buf[64];
    e_addr_name_source name_source;
    const void *extra_data = NULL;

    get_trusted_name_IgnoreAndReturn(&tn);
    get_address_book_contact_IgnoreAndReturn(NULL);

    TEST_ASSERT_TRUE(get_address_display_name(TEST_ADDR,
                                              CHAIN_ID,
                                              1,
                                              TYPES_ACCOUNT,
                                              1,
                                              SOURCES_ENS,
                                              buf,
                                              sizeof(buf),
                                              &name_source,
                                              &extra_data));

    TEST_ASSERT_EQUAL(name_source, ADDR_NAME_FROM_TRUSTED_NAME);
    TEST_ASSERT_EQUAL_STRING(buf, "alice.eth");
    TEST_ASSERT_EQUAL_PTR(extra_data, &tn);
}

/**
 * Address Book contact whose name is empty → not a match, the checksummed address is used
 */
void test_ab_empty_name_falls_back_to_raw(void) {
    static s_ab_contact ab = {.contact_name = ""};
    char buf[64];
    e_addr_name_source name_source;
    const void *extra_data = &ab;

    get_trusted_name_IgnoreAndReturn(NULL);
    get_address_book_contact_IgnoreAndReturn(&ab);

    TEST_ASSERT_TRUE(get_address_display_name(TEST_ADDR,
                                              CHAIN_ID,
                                              1,
                                              TYPES_ACCOUNT,
                                              1,
                                              SOURCES_ENS,
                                              buf,
                                              sizeof(buf),
                                              &name_source,
                                              &extra_data));

    TEST_ASSERT_EQUAL(ADDR_NAME_FROM_RAW, name_source);
    TEST_ASSERT_EQUAL_STRING("0x11223344556677889900aabbccddeEfF11223344", buf);
    TEST_ASSERT_NULL(extra_data);
}

/**
 * Space-only contact name → not a match either: it renders as a blank row that
 * occupies space, which reads as "an address was shown" rather than "the field is missing"
 */
void test_ab_blank_name_falls_back_to_raw(void) {
    static s_ab_contact ab = {.contact_name = "   "};
    char buf[64];
    e_addr_name_source name_source;

    get_trusted_name_IgnoreAndReturn(NULL);
    get_address_book_contact_IgnoreAndReturn(&ab);

    TEST_ASSERT_TRUE(get_address_display_name(TEST_ADDR,
                                              CHAIN_ID,
                                              1,
                                              TYPES_ACCOUNT,
                                              1,
                                              SOURCES_ENS,
                                              buf,
                                              sizeof(buf),
                                              &name_source,
                                              NULL));

    TEST_ASSERT_EQUAL(ADDR_NAME_FROM_RAW, name_source);
    TEST_ASSERT_EQUAL_STRING("0x11223344556677889900aabbccddeEfF11223344", buf);
}

/**
 * Empty contact name but a usable Trusted Name → the Trusted Name is used, not a blank
 */
void test_ab_empty_name_falls_back_to_trusted_name(void) {
    static s_ab_contact ab = {.contact_name = ""};
    static s_trusted_name tn = {.name = "alice.eth"};
    char buf[64];
    e_addr_name_source name_source;

    get_trusted_name_IgnoreAndReturn(&tn);
    get_address_book_contact_IgnoreAndReturn(&ab);

    TEST_ASSERT_TRUE(get_address_display_name(TEST_ADDR,
                                              CHAIN_ID,
                                              1,
                                              TYPES_ACCOUNT,
                                              1,
                                              SOURCES_ENS,
                                              buf,
                                              sizeof(buf),
                                              &name_source,
                                              NULL));

    TEST_ASSERT_EQUAL(ADDR_NAME_FROM_TRUSTED_NAME, name_source);
    TEST_ASSERT_EQUAL_STRING("alice.eth", buf);
}

/**
 * Trusted Name whose name is empty → not a match, the checksummed address is used
 */
void test_tn_empty_name_falls_back_to_raw(void) {
    static s_trusted_name tn = {.name = ""};
    char buf[64];
    e_addr_name_source name_source;

    get_trusted_name_IgnoreAndReturn(&tn);
    get_address_book_contact_IgnoreAndReturn(NULL);

    TEST_ASSERT_TRUE(get_address_display_name(TEST_ADDR,
                                              CHAIN_ID,
                                              1,
                                              TYPES_ACCOUNT,
                                              1,
                                              SOURCES_ENS,
                                              buf,
                                              sizeof(buf),
                                              &name_source,
                                              NULL));

    TEST_ASSERT_EQUAL(ADDR_NAME_FROM_RAW, name_source);
    TEST_ASSERT_EQUAL_STRING("0x11223344556677889900aabbccddeEfF11223344", buf);
}

/**
 * Contact name carrying a character the screen cannot render → not a match
 */
void test_ab_unprintable_name_falls_back_to_raw(void) {
    static s_ab_contact ab = {.contact_name = "Ali\tce"};
    char buf[64];
    e_addr_name_source name_source;

    get_trusted_name_IgnoreAndReturn(NULL);
    get_address_book_contact_IgnoreAndReturn(&ab);

    TEST_ASSERT_TRUE(get_address_display_name(TEST_ADDR,
                                              CHAIN_ID,
                                              1,
                                              TYPES_ACCOUNT,
                                              1,
                                              SOURCES_ENS,
                                              buf,
                                              sizeof(buf),
                                              &name_source,
                                              NULL));

    TEST_ASSERT_EQUAL(ADDR_NAME_FROM_RAW, name_source);
    TEST_ASSERT_EQUAL_STRING("0x11223344556677889900aabbccddeEfF11223344", buf);
}

/**
 * Neither Address Book nor Trusted Name found → ADDR_NAME_FROM_RAW, formatted address in buf
 */
void test_raw_fallback(void) {
    char buf[64];
    e_addr_name_source name_source;

    get_trusted_name_IgnoreAndReturn(NULL);
    get_address_book_contact_IgnoreAndReturn(NULL);

    TEST_ASSERT_TRUE(get_address_display_name(TEST_ADDR,
                                              CHAIN_ID,
                                              1,
                                              TYPES_ACCOUNT,
                                              1,
                                              SOURCES_ENS,
                                              buf,
                                              sizeof(buf),
                                              &name_source,
                                              NULL));

    TEST_ASSERT_EQUAL(name_source, ADDR_NAME_FROM_RAW);
    TEST_ASSERT_EQUAL_STRING(buf, "0x11223344556677889900aabbccddeEfF11223344");
}

/**
 * TN_TYPE_ACCOUNT not in types → Address Book is not checked; Trusted Name found →
 * ADDR_NAME_FROM_TRUSTED_NAME
 */
void test_non_account_type_skips_ab(void) {
    static const e_name_type types_contract[] = {TN_TYPE_CONTRACT};
    static s_trusted_name tn = {.name = "MyContract"};
    char buf[64];
    e_addr_name_source name_source;

    get_trusted_name_IgnoreAndReturn(&tn);
    // get_address_book_contact is not set up: CMock will fail if called

    TEST_ASSERT_TRUE(get_address_display_name(TEST_ADDR,
                                              CHAIN_ID,
                                              1,
                                              types_contract,
                                              1,
                                              SOURCES_ENS,
                                              buf,
                                              sizeof(buf),
                                              &name_source,
                                              NULL));

    TEST_ASSERT_EQUAL(name_source, ADDR_NAME_FROM_TRUSTED_NAME);
    TEST_ASSERT_EQUAL_STRING(buf, "MyContract");
}

/**
 * Wrong chain_id (e.g. Polygon 137 instead of Ethereum 1): neither Address Book nor Trusted Name
 * resolves the address for a chain the entries were not registered on → RAW
 */
void test_wrong_chain_id(void) {
    const uint64_t polygon_chain_id = 137;
    char buf[64];
    e_addr_name_source name_source;

    // In the real implementation, get_trusted_name checks chain_id equality
    // and get_address_book_contact checks the contact's chain scope.
    // Both return NULL when the chain doesn't match.
    get_trusted_name_IgnoreAndReturn(NULL);
    get_address_book_contact_IgnoreAndReturn(NULL);

    TEST_ASSERT_TRUE(get_address_display_name(TEST_ADDR,
                                              polygon_chain_id,
                                              1,
                                              TYPES_ACCOUNT,
                                              1,
                                              SOURCES_ENS,
                                              buf,
                                              sizeof(buf),
                                              &name_source,
                                              NULL));

    TEST_ASSERT_EQUAL(name_source, ADDR_NAME_FROM_RAW);
    TEST_ASSERT_EQUAL_STRING(buf, "0x11223344556677889900aabbccddeEfF11223344");
}

/**
 * Non-Ethereum blockchain family (chain_id=0): chain_is_ethereum_compatible
 * returns false inside get_trusted_name, so neither Address Book nor Trusted Name resolves → RAW
 */
void test_non_ethereum_chain(void) {
    const uint64_t non_eth_chain_id = 0;
    char buf[64];
    e_addr_name_source name_source;

    // In the real implementation, get_trusted_name rejects non-Ethereum-compatible
    // chains (chain_is_ethereum_compatible == false) and returns NULL.
    get_trusted_name_IgnoreAndReturn(NULL);
    get_address_book_contact_IgnoreAndReturn(NULL);

    TEST_ASSERT_TRUE(get_address_display_name(TEST_ADDR,
                                              non_eth_chain_id,
                                              1,
                                              TYPES_ACCOUNT,
                                              1,
                                              SOURCES_ENS,
                                              buf,
                                              sizeof(buf),
                                              &name_source,
                                              NULL));

    TEST_ASSERT_EQUAL(name_source, ADDR_NAME_FROM_RAW);
    TEST_ASSERT_EQUAL_STRING(buf, "0x11223344556677889900aabbccddeEfF11223344");
}

// =============================================================================
// Test runner
// =============================================================================

void setUp(void) {
    Mocktrusted_name_Init();
    Mockhandle_contacts_Init();
}

void tearDown(void) {
    Mocktrusted_name_Verify();
    Mocktrusted_name_Destroy();
    Mockhandle_contacts_Verify();
    Mockhandle_contacts_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_conflict_ab_and_tn);
    RUN_TEST(test_ab_only);
    RUN_TEST(test_tn_only);
    RUN_TEST(test_ab_empty_name_falls_back_to_raw);
    RUN_TEST(test_ab_blank_name_falls_back_to_raw);
    RUN_TEST(test_ab_empty_name_falls_back_to_trusted_name);
    RUN_TEST(test_tn_empty_name_falls_back_to_raw);
    RUN_TEST(test_ab_unprintable_name_falls_back_to_raw);
    RUN_TEST(test_raw_fallback);
    RUN_TEST(test_non_account_type_skips_ab);
    RUN_TEST(test_wrong_chain_id);
    RUN_TEST(test_non_ethereum_chain);
    return UNITY_END();
}
