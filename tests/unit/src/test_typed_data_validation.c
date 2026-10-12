/**
 * @file test_typed_data_validation.c
 * @brief Unit tests for what the app accepts of an EIP-712 V2 schema and its values, at
 *        src/features/sign_message_eip712/typed_data.c
 *
 * What a name must not hold is what would make a schema ambiguous: how it looks is for the review
 * to judge. A leaf has one encoding per type. Both come from the host, so each rule is checked on
 * its boundary: the last byte accepted and the first one refused.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "typed_data.h"

// the hash pass is not what these tests are about
bool value_hash_pass(const s_eip712_impl *impl) {
    (void) impl;
    return true;
}

bool value_hash_domain(const s_eip712_impl *impl, uint8_t out[CX_KECCAK_256_SIZE]) {
    (void) impl;
    (void) out;
    return true;
}

// =============================================================================
// Names
// =============================================================================

static bool struct_name_ok(const char *name) {
    return td_is_valid_struct_name((const uint8_t *) name, strlen(name));
}

static bool field_name_ok(const char *name) {
    return td_is_valid_field_name((const uint8_t *) name, strlen(name));
}

void test_an_identifier_is_a_name(void) {
    const char *names[] =
        {"a", "Mail", "EIP712Domain", "verifyingContract", "a1", "_", "$", "_a$1"};

    for (size_t i = 0; i < ARRAYLEN(names); ++i) {
        TEST_ASSERT_TRUE_MESSAGE(struct_name_ok(names[i]), names[i]);
        TEST_ASSERT_TRUE_MESSAGE(field_name_ok(names[i]), names[i]);
    }
}

void test_a_name_does_not_have_to_be_an_identifier(void) {
    // the standard asks for an identifier for a struct, and hosts send others: the first one is
    // a primary type of Hyperliquid
    const char *names[] = {"HyperliquidTransaction:UsdSend", "1inch", "a-b", "a.b", "a:b", "1st"};

    for (size_t i = 0; i < ARRAYLEN(names); ++i) {
        TEST_ASSERT_TRUE_MESSAGE(struct_name_ok(names[i]), names[i]);
        TEST_ASSERT_TRUE_MESSAGE(field_name_ok(names[i]), names[i]);
    }
}

void test_a_name_cannot_be_empty(void) {
    TEST_ASSERT_FALSE(td_is_valid_struct_name((const uint8_t *) "", 0));
    TEST_ASSERT_FALSE(td_is_valid_struct_name(NULL, 0));
    TEST_ASSERT_FALSE(td_is_valid_struct_name(NULL, 3));
    TEST_ASSERT_FALSE(td_is_valid_field_name((const uint8_t *) "", 0));
    TEST_ASSERT_FALSE(td_is_valid_field_name(NULL, 0));
    TEST_ASSERT_FALSE(td_is_valid_field_name(NULL, 3));
}

void test_the_delimiters_of_the_type_string_are_refused_everywhere(void) {
    const char *names[] = {"a,b", "a(b", "a)b", ",", "(", ")"};

    for (size_t i = 0; i < ARRAYLEN(names); ++i) {
        TEST_ASSERT_FALSE_MESSAGE(struct_name_ok(names[i]), names[i]);
        TEST_ASSERT_FALSE_MESSAGE(field_name_ok(names[i]), names[i]);
    }
}

void test_what_breaks_the_json_of_the_schema_is_refused_everywhere(void) {
    const char *names[] = {"a\"b", "a\\b", "\"", "\\"};

    for (size_t i = 0; i < ARRAYLEN(names); ++i) {
        TEST_ASSERT_FALSE_MESSAGE(struct_name_ok(names[i]), names[i]);
        TEST_ASSERT_FALSE_MESSAGE(field_name_ok(names[i]), names[i]);
    }
}

void test_a_nul_cuts_a_name_short_so_it_is_refused(void) {
    const uint8_t inside[] = {'a', 0x00, 'b'};
    const uint8_t alone[] = {0x00};

    TEST_ASSERT_FALSE(td_is_valid_struct_name(inside, sizeof(inside)));
    TEST_ASSERT_FALSE(td_is_valid_struct_name(alone, sizeof(alone)));
    TEST_ASSERT_FALSE(td_is_valid_field_name(inside, sizeof(inside)));
    TEST_ASSERT_FALSE(td_is_valid_field_name(alone, sizeof(alone)));
}

void test_the_brackets_and_the_space_are_for_field_names_only(void) {
    TEST_ASSERT_FALSE(struct_name_ok("a b"));
    TEST_ASSERT_FALSE(struct_name_ok(" "));
    TEST_ASSERT_FALSE(struct_name_ok("a[0]"));
    TEST_ASSERT_FALSE(struct_name_ok("a[]"));
    TEST_ASSERT_FALSE(struct_name_ok("]"));
    TEST_ASSERT_TRUE(field_name_ok("a b"));
    TEST_ASSERT_TRUE(field_name_ok("Order Details"));
    TEST_ASSERT_TRUE(field_name_ok("a[0]"));
}

void test_how_a_name_looks_is_not_for_this_module(void) {
    // the review tells what it can show: a name only has to keep the schema unambiguous
    const uint8_t accented[] = {'c', 'a', 'f', 0xC3, 0xA9};
    const uint8_t control[] = {'a', 0x01, 'b'};
    const uint8_t del[] = {'a', 0x7F, 'b'};
    const uint8_t high_only[] = {0x80};

    TEST_ASSERT_TRUE(td_is_valid_struct_name(accented, sizeof(accented)));
    TEST_ASSERT_TRUE(td_is_valid_field_name(accented, sizeof(accented)));
    TEST_ASSERT_TRUE(td_is_valid_struct_name(control, sizeof(control)));
    TEST_ASSERT_TRUE(td_is_valid_field_name(control, sizeof(control)));
    TEST_ASSERT_TRUE(td_is_valid_struct_name(del, sizeof(del)));
    TEST_ASSERT_TRUE(td_is_valid_field_name(del, sizeof(del)));
    TEST_ASSERT_TRUE(td_is_valid_struct_name(high_only, sizeof(high_only)));
    TEST_ASSERT_TRUE(td_is_valid_field_name(high_only, sizeof(high_only)));
}

void test_the_length_given_is_the_one_checked(void) {
    // the byte after the length is not part of the name
    TEST_ASSERT_TRUE(td_is_valid_struct_name((const uint8_t *) "ab,c", 2));
    TEST_ASSERT_FALSE(td_is_valid_struct_name((const uint8_t *) "ab,c", 3));
    TEST_ASSERT_TRUE(td_is_valid_field_name((const uint8_t *) "ab,c", 2));
    TEST_ASSERT_FALSE(td_is_valid_field_name((const uint8_t *) "ab,c", 3));
}

// =============================================================================
// Leaves
// =============================================================================

static s_struct_712_field *new_sized_field(e_type type, uint8_t size) {
    s_struct_712_field *field = td_create_field_def(type);

    TEST_ASSERT_NOT_NULL(field);
    TEST_ASSERT_TRUE(td_field_set_key_name(field, (const uint8_t *) "x", 1));
    if (size > 0) {
        TEST_ASSERT_TRUE(td_field_set_type_size(field, size));
    }
    return field;
}

static bool leaf_ok(e_type type, uint8_t size, const uint8_t *value, size_t length) {
    s_struct_712_field *field = new_sized_field(type, size);
    const bool ok = td_is_valid_leaf(field, value, length);

    td_discard_struct_field(field);
    return ok;
}

#define LEAF_OK(type, size, ...)                                                           \
    do {                                                                                   \
        const uint8_t value[] = {__VA_ARGS__};                                             \
        TEST_ASSERT_TRUE_MESSAGE(leaf_ok(type, size, value, sizeof(value)), #__VA_ARGS__); \
    } while (0)

#define LEAF_REFUSED(type, size, ...)                                                       \
    do {                                                                                    \
        const uint8_t value[] = {__VA_ARGS__};                                              \
        TEST_ASSERT_FALSE_MESSAGE(leaf_ok(type, size, value, sizeof(value)), #__VA_ARGS__); \
    } while (0)

void test_no_field_and_no_bytes_are_refused(void) {
    const uint8_t one = 1;

    TEST_ASSERT_FALSE(td_is_valid_leaf(NULL, &one, 1));
}

void test_a_missing_buffer_is_only_an_empty_value(void) {
    s_struct_712_field *string = new_sized_field(TYPE_SOL_STRING, 0);
    s_struct_712_field *number = new_sized_field(TYPE_SOL_UINT, 32);

    TEST_ASSERT_TRUE(td_is_valid_leaf(string, NULL, 0));
    TEST_ASSERT_FALSE(td_is_valid_leaf(string, NULL, 1));
    TEST_ASSERT_FALSE(td_is_valid_leaf(number, NULL, 0));
    td_discard_struct_field(string);
    td_discard_struct_field(number);
}

void test_an_unsigned_integer_is_minimal_big_endian(void) {
    LEAF_OK(TYPE_SOL_UINT, 32, 0x00);
    LEAF_OK(TYPE_SOL_UINT, 32, 0x01);
    LEAF_OK(TYPE_SOL_UINT, 32, 0xFF);
    LEAF_OK(TYPE_SOL_UINT, 32, 0x01, 0x00);
    LEAF_OK(TYPE_SOL_UINT, 32, 0xFF, 0xFF, 0xFF);
}

void test_an_unsigned_integer_with_a_leading_zero_is_refused(void) {
    LEAF_REFUSED(TYPE_SOL_UINT, 32, 0x00, 0x00);
    LEAF_REFUSED(TYPE_SOL_UINT, 32, 0x00, 0x01);
    LEAF_REFUSED(TYPE_SOL_UINT, 32, 0x00, 0xFF);
    LEAF_REFUSED(TYPE_SOL_UINT, 32, 0x00, 0x01, 0x02);
}

void test_an_unsigned_integer_cannot_be_empty(void) {
    s_struct_712_field *field = new_sized_field(TYPE_SOL_UINT, 32);
    const uint8_t none = 0;

    TEST_ASSERT_FALSE(td_is_valid_leaf(field, &none, 0));
    td_discard_struct_field(field);
}

void test_an_unsigned_integer_is_no_longer_than_its_type(void) {
    uint8_t max[32];
    uint8_t too_long[33];

    // uint8 holds one byte, uint16 two, uint256 thirty-two
    LEAF_OK(TYPE_SOL_UINT, 1, 0xFF);
    LEAF_REFUSED(TYPE_SOL_UINT, 1, 0x01, 0x00);
    LEAF_OK(TYPE_SOL_UINT, 2, 0xFF, 0xFF);
    LEAF_REFUSED(TYPE_SOL_UINT, 2, 0x01, 0x00, 0x00);

    memset(max, 0xFF, sizeof(max));
    memset(too_long, 0xFF, sizeof(too_long));
    TEST_ASSERT_TRUE(leaf_ok(TYPE_SOL_UINT, 32, max, sizeof(max)));
    TEST_ASSERT_FALSE(leaf_ok(TYPE_SOL_UINT, 32, too_long, sizeof(too_long)));
}

void test_a_signed_integer_is_minimal_while_not_negative(void) {
    LEAF_OK(TYPE_SOL_INT, 4, 0x00);
    LEAF_OK(TYPE_SOL_INT, 4, 0x7F);
    // the top bit alone does not make a short value negative
    LEAF_OK(TYPE_SOL_INT, 4, 0x80);
    LEAF_OK(TYPE_SOL_INT, 4, 0xFF);
    LEAF_OK(TYPE_SOL_INT, 4, 0x01, 0x00, 0x00);
    LEAF_REFUSED(TYPE_SOL_INT, 4, 0x00, 0x7F);
    LEAF_REFUSED(TYPE_SOL_INT, 4, 0x00, 0x80);
    LEAF_REFUSED(TYPE_SOL_INT, 4, 0x00, 0x00, 0x00, 0x01);
}

void test_a_negative_integer_has_the_full_width_of_its_type(void) {
    LEAF_OK(TYPE_SOL_INT, 4, 0xFF, 0xFF, 0xFF, 0xFF);
    LEAF_OK(TYPE_SOL_INT, 4, 0x80, 0x00, 0x00, 0x00);
    LEAF_OK(TYPE_SOL_INT, 1, 0xFF);
    LEAF_OK(TYPE_SOL_INT, 1, 0x80);
    // a positive of the full width has no top bit, and no leading zero either
    LEAF_OK(TYPE_SOL_INT, 4, 0x7F, 0xFF, 0xFF, 0xFF);
    LEAF_REFUSED(TYPE_SOL_INT, 4, 0x00, 0xFF, 0xFF, 0xFF);
}

void test_a_signed_integer_is_no_longer_than_its_type(void) {
    LEAF_REFUSED(TYPE_SOL_INT, 4, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    LEAF_REFUSED(TYPE_SOL_INT, 1, 0x01, 0x00);
    LEAF_REFUSED(TYPE_SOL_INT, 4, 0x01, 0x00, 0x00, 0x00, 0x00);
}

void test_a_signed_integer_cannot_be_empty(void) {
    s_struct_712_field *field = new_sized_field(TYPE_SOL_INT, 4);
    const uint8_t none = 0;

    TEST_ASSERT_FALSE(td_is_valid_leaf(field, &none, 0));
    td_discard_struct_field(field);
}

void test_fixed_bytes_have_exactly_their_size(void) {
    LEAF_OK(TYPE_SOL_BYTES_FIX, 1, 0x00);
    LEAF_OK(TYPE_SOL_BYTES_FIX, 4, 0xDE, 0xAD, 0xBE, 0xEF);
    LEAF_OK(TYPE_SOL_BYTES_FIX, 4, 0x00, 0x00, 0x00, 0x00);
    LEAF_REFUSED(TYPE_SOL_BYTES_FIX, 4, 0xDE, 0xAD, 0xBE);
    LEAF_REFUSED(TYPE_SOL_BYTES_FIX, 4, 0xDE, 0xAD, 0xBE, 0xEF, 0x00);
    LEAF_REFUSED(TYPE_SOL_BYTES_FIX, 1, 0x00, 0x00);
}

void test_fixed_bytes_cannot_be_empty(void) {
    s_struct_712_field *field = new_sized_field(TYPE_SOL_BYTES_FIX, 4);
    const uint8_t none = 0;

    TEST_ASSERT_FALSE(td_is_valid_leaf(field, &none, 0));
    td_discard_struct_field(field);
}

void test_an_address_has_twenty_bytes(void) {
    uint8_t address[21] = {0};

    TEST_ASSERT_TRUE(leaf_ok(TYPE_SOL_ADDRESS, 0, address, ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(leaf_ok(TYPE_SOL_ADDRESS, 0, address, ADDRESS_LENGTH - 1));
    TEST_ASSERT_FALSE(leaf_ok(TYPE_SOL_ADDRESS, 0, address, ADDRESS_LENGTH + 1));
    TEST_ASSERT_FALSE(leaf_ok(TYPE_SOL_ADDRESS, 0, address, 0));
}

void test_a_boolean_is_one_byte_zero_or_one(void) {
    LEAF_OK(TYPE_SOL_BOOL, 0, 0x00);
    LEAF_OK(TYPE_SOL_BOOL, 0, 0x01);
    LEAF_REFUSED(TYPE_SOL_BOOL, 0, 0x02);
    LEAF_REFUSED(TYPE_SOL_BOOL, 0, 0xFF);
    LEAF_REFUSED(TYPE_SOL_BOOL, 0, 0x00, 0x00);
    LEAF_REFUSED(TYPE_SOL_BOOL, 0, 0x00, 0x01);
    LEAF_REFUSED(TYPE_SOL_BOOL, 0, 0x01, 0x00);
}

void test_an_empty_boolean_is_refused(void) {
    s_struct_712_field *field = new_sized_field(TYPE_SOL_BOOL, 0);
    const uint8_t none = 0;

    TEST_ASSERT_FALSE(td_is_valid_leaf(field, &none, 0));
    td_discard_struct_field(field);
}

void test_a_string_and_dynamic_bytes_can_be_anything(void) {
    const uint8_t some[] = {0x00, 0xFF, 0x80, 0x00};
    const uint8_t none = 0;

    TEST_ASSERT_TRUE(leaf_ok(TYPE_SOL_STRING, 0, some, sizeof(some)));
    TEST_ASSERT_TRUE(leaf_ok(TYPE_SOL_STRING, 0, &none, 0));
    TEST_ASSERT_TRUE(leaf_ok(TYPE_SOL_BYTES_DYN, 0, some, sizeof(some)));
    TEST_ASSERT_TRUE(leaf_ok(TYPE_SOL_BYTES_DYN, 0, &none, 0));
}

void test_a_struct_is_not_a_leaf(void) {
    const uint8_t one = 1;

    TEST_ASSERT_FALSE(leaf_ok(TYPE_STRUCT, 0, &one, 1));
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    td_deinit();
}

void tearDown(void) {
    td_deinit();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_an_identifier_is_a_name);
    RUN_TEST(test_a_name_does_not_have_to_be_an_identifier);
    RUN_TEST(test_a_name_cannot_be_empty);
    RUN_TEST(test_the_delimiters_of_the_type_string_are_refused_everywhere);
    RUN_TEST(test_what_breaks_the_json_of_the_schema_is_refused_everywhere);
    RUN_TEST(test_a_nul_cuts_a_name_short_so_it_is_refused);
    RUN_TEST(test_the_brackets_and_the_space_are_for_field_names_only);
    RUN_TEST(test_how_a_name_looks_is_not_for_this_module);
    RUN_TEST(test_the_length_given_is_the_one_checked);
    RUN_TEST(test_no_field_and_no_bytes_are_refused);
    RUN_TEST(test_a_missing_buffer_is_only_an_empty_value);
    RUN_TEST(test_an_unsigned_integer_is_minimal_big_endian);
    RUN_TEST(test_an_unsigned_integer_with_a_leading_zero_is_refused);
    RUN_TEST(test_an_unsigned_integer_cannot_be_empty);
    RUN_TEST(test_an_unsigned_integer_is_no_longer_than_its_type);
    RUN_TEST(test_a_signed_integer_is_minimal_while_not_negative);
    RUN_TEST(test_a_negative_integer_has_the_full_width_of_its_type);
    RUN_TEST(test_a_signed_integer_is_no_longer_than_its_type);
    RUN_TEST(test_a_signed_integer_cannot_be_empty);
    RUN_TEST(test_fixed_bytes_have_exactly_their_size);
    RUN_TEST(test_fixed_bytes_cannot_be_empty);
    RUN_TEST(test_an_address_has_twenty_bytes);
    RUN_TEST(test_a_boolean_is_one_byte_zero_or_one);
    RUN_TEST(test_an_empty_boolean_is_refused);
    RUN_TEST(test_a_string_and_dynamic_bytes_can_be_anything);
    RUN_TEST(test_a_struct_is_not_a_leaf);
    return UNITY_END();
}
