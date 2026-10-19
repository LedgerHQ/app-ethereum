/**
 * @file test_typed_data_domain.c
 * @brief Unit tests for the chain ID that an EIP-712 domain holds, at
 *        src/features/sign_message_eip712/typed_data.c
 *
 * The domain is the one a host sends, so its chainId can be of any type. Only an unsigned integer
 * is a chain ID: the bytes of any other type are not a number.
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

// Sets the domain to the struct { <type> chainId; } holding the given bytes
static void build_domain(e_type type, size_t type_size, const uint8_t *data, size_t length) {
    s_struct_712 *def = td_create_struct_def("EIP712Domain");
    s_struct_712_field *field = td_create_field_def(type);
    s_struct_712_value *domain;
    s_struct_712_value *leaf;

    TEST_ASSERT_NOT_NULL(def);
    TEST_ASSERT_NOT_NULL(field);
    TEST_ASSERT_TRUE(td_field_set_key_name(field, (const uint8_t *) "chainId", 7));
    if (type_size > 0) {
        TEST_ASSERT_TRUE(td_field_set_type_size(field, type_size));
    }
    TEST_ASSERT_TRUE(td_add_struct_field_def(def, field));
    TEST_ASSERT_TRUE(td_add_struct_def(def));

    def = (s_struct_712 *) td_find_struct("EIP712Domain");
    domain = td_create_struct(def);
    TEST_ASSERT_NOT_NULL(domain);
    leaf = td_create_leaf(def->fields, length);
    TEST_ASSERT_NOT_NULL(leaf);
    TEST_ASSERT_TRUE(td_leaf_write(leaf, 0, data, length));
    TEST_ASSERT_TRUE(td_append_child(domain, leaf));
    TEST_ASSERT_TRUE(td_set_domain(domain));
}

void test_no_domain_has_no_chain_id(void) {
    uint64_t chain_id = 0x1234;

    TEST_ASSERT_FALSE(td_get_domain_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(0x1234, chain_id);
}

void test_a_uint256_chain_id_is_read(void) {
    uint8_t chain[32] = {0};
    uint64_t chain_id = 0;

    chain[31] = 0x01;
    build_domain(TYPE_SOL_UINT, 32, chain, sizeof(chain));
    TEST_ASSERT_TRUE(td_get_domain_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(1, chain_id);
}

void test_a_smaller_uint_chain_id_is_read(void) {
    const uint8_t chain[] = {0x01, 0x02};
    uint64_t chain_id = 0;

    build_domain(TYPE_SOL_UINT, 2, chain, sizeof(chain));
    TEST_ASSERT_TRUE(td_get_domain_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(0x0102, chain_id);
}

void test_a_chain_id_wider_than_64_bits_is_refused_unless_it_is_padding(void) {
    uint8_t chain[32] = {0};
    uint64_t chain_id = 0x1234;

    chain[0] = 0x01;
    chain[31] = 0x01;
    build_domain(TYPE_SOL_UINT, 32, chain, sizeof(chain));
    TEST_ASSERT_FALSE(td_get_domain_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(0x1234, chain_id);
}

// "1" is not the chain 0x31, and 32 bytes that end with 0x01 are not the chain 1
void test_a_chain_id_of_another_type_is_not_a_chain(void) {
    const struct {
        e_type type;
        size_t type_size;
        uint8_t data[32];
        size_t length;
    } cases[] = {
        {TYPE_SOL_STRING, 0, {'1'}, 1},
        {TYPE_SOL_INT, 32, {[31] = 0x01}, 32},
        {TYPE_SOL_BOOL, 0, {0x01}, 1},
        {TYPE_SOL_ADDRESS, 0, {[19] = 0x01}, 20},
        {TYPE_SOL_BYTES_FIX, 32, {[31] = 0x01}, 32},
        {TYPE_SOL_BYTES_DYN, 0, {0x01}, 1},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint64_t chain_id = 0x1234;

        td_deinit();
        build_domain(cases[i].type, cases[i].type_size, cases[i].data, cases[i].length);
        TEST_ASSERT_FALSE(td_get_domain_chain_id(&chain_id));
        TEST_ASSERT_EQUAL_UINT64(0x1234, chain_id);
    }
}

void setUp(void) {
    td_deinit();
}

void tearDown(void) {
    td_deinit();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_domain_has_no_chain_id);
    RUN_TEST(test_a_uint256_chain_id_is_read);
    RUN_TEST(test_a_smaller_uint_chain_id_is_read);
    RUN_TEST(test_a_chain_id_wider_than_64_bits_is_refused_unless_it_is_padding);
    RUN_TEST(test_a_chain_id_of_another_type_is_not_a_chain);
    return UNITY_END();
}
