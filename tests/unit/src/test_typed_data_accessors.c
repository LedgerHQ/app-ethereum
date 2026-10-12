/**
 * @file test_typed_data_accessors.c
 * @brief Unit tests for what the descriptors of an EIP-712 V2 message read from its value trees,
 *        at src/features/sign_message_eip712/typed_data.c
 *
 * That is the root of the message, the children of a node, and the chain and the contract of the
 * domain. The domain is the one an app is handed by a host: any of its values can be missing, or
 * have a type that is not the expected one, and what a descriptor takes from it must be refused
 * rather than guessed.
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
// Trees
// =============================================================================

static s_struct_712_field *new_field(e_type type, const char *name) {
    s_struct_712_field *field = td_create_field_def(type);

    TEST_ASSERT_NOT_NULL(field);
    TEST_ASSERT_TRUE(td_field_set_key_name(field, (const uint8_t *) name, strlen(name)));
    return field;
}

static s_struct_712_field *new_sized_field(e_type type, uint8_t size, const char *name) {
    s_struct_712_field *field = new_field(type, name);

    TEST_ASSERT_TRUE(td_field_set_type_size(field, size));
    return field;
}

static s_struct_712 *new_struct_def(const char *name) {
    s_struct_712 *def = td_create_struct_def((const uint8_t *) name, strlen(name));

    TEST_ASSERT_NOT_NULL(def);
    return def;
}

static void add_leaf(s_struct_712_value *parent,
                     const s_struct_712_field *field,
                     const uint8_t *data,
                     size_t length) {
    s_struct_712_value *leaf = td_create_leaf(field, length);

    TEST_ASSERT_NOT_NULL(leaf);
    TEST_ASSERT_TRUE(td_leaf_write(leaf, 0, data, length));
    TEST_ASSERT_TRUE(td_append_child(parent, leaf));
}

static const s_struct_712_field *field_at(const s_struct_712 *def, size_t index) {
    const s_struct_712_field *field = def->fields;

    for (size_t i = 0; i < index; ++i) {
        field = (const s_struct_712_field *) ((const flist_node_t *) field)->next;
    }
    return field;
}

// What the domain holds, and how: a value is missing when its pointer is NULL and its length is
// not. The chain is an unsigned integer unless chain_id_type says otherwise.
typedef struct {
    bool has_chain_id;
    e_type chain_id_type;
    const uint8_t *chain_id;
    size_t chain_id_length;
    bool has_contract;
    e_type contract_type;
    const uint8_t *contract;
    size_t contract_length;
} s_domain_spec;

#define DOMAIN_FIELD_NAME     0
#define DOMAIN_FIELD_CHAIN_ID 1
#define DOMAIN_FIELD_CONTRACT 2

static s_struct_712_field *new_chain_id_field(e_type type) {
    switch (type) {
        case TYPE_SOL_INT:
        case TYPE_SOL_UINT:
        case TYPE_SOL_BYTES_FIX:
            return new_sized_field(type, 32, "chainId");
        case TYPE_STRUCT:
            // 0, what an unset chain_id_type holds: the default type
            return new_sized_field(TYPE_SOL_UINT, 32, "chainId");
        default:
            return new_field(type, "chainId");
    }
}

static void build_domain(const s_domain_spec *spec) {
    s_struct_712 *def = new_struct_def("EIP712Domain");
    s_struct_712_value *domain;
    size_t index = 0;

    TEST_ASSERT_TRUE(td_add_struct_field_def(def, new_field(TYPE_SOL_STRING, "name")));
    if (spec->has_chain_id) {
        TEST_ASSERT_TRUE(td_add_struct_field_def(def, new_chain_id_field(spec->chain_id_type)));
    }
    if (spec->has_contract) {
        s_struct_712_field *field = new_field(spec->contract_type, "verifyingContract");

        if (spec->contract_type == TYPE_SOL_UINT) {
            TEST_ASSERT_TRUE(td_field_set_type_size(field, 32));
        }
        TEST_ASSERT_TRUE(td_add_struct_field_def(def, field));
    }
    TEST_ASSERT_TRUE(td_add_struct_def(def));

    def = (s_struct_712 *) td_find_struct("EIP712Domain");
    domain = td_create_struct(def);
    TEST_ASSERT_NOT_NULL(domain);
    add_leaf(domain, field_at(def, index++), (const uint8_t *) "Test", 4);
    if (spec->has_chain_id) {
        add_leaf(domain, field_at(def, index++), spec->chain_id, spec->chain_id_length);
    }
    if (spec->has_contract) {
        add_leaf(domain, field_at(def, index++), spec->contract, spec->contract_length);
    }
    TEST_ASSERT_TRUE(td_set_domain(domain));
}

// Order { address owner; uint8[] ids; } with three ids
static void build_message(void) {
    s_struct_712 *def = new_struct_def("Order");
    s_struct_712_field *ids = new_sized_field(TYPE_SOL_UINT, 1, "ids");
    s_struct_712_value *order;
    s_struct_712_value *array;
    uint8_t owner[ADDRESS_LENGTH] = {0xAA};

    TEST_ASSERT_TRUE(td_add_struct_field_def(def, new_field(TYPE_SOL_ADDRESS, "owner")));
    TEST_ASSERT_TRUE(td_field_set_array_level_count(ids, 1));
    TEST_ASSERT_TRUE(td_field_set_array_level(ids, 0, ARRAY_DYNAMIC, 0));
    TEST_ASSERT_TRUE(td_add_struct_field_def(def, ids));
    TEST_ASSERT_TRUE(td_add_struct_def(def));

    def = (s_struct_712 *) td_find_struct("Order");
    order = td_create_struct(def);
    array = td_create_array(field_at(def, 1));
    TEST_ASSERT_NOT_NULL(order);
    TEST_ASSERT_NOT_NULL(array);
    add_leaf(order, field_at(def, 0), owner, sizeof(owner));
    for (uint8_t id = 1; id <= 3; ++id) {
        add_leaf(array, field_at(def, 1), &id, sizeof(id));
    }
    TEST_ASSERT_TRUE(td_append_child(order, array));
    TEST_ASSERT_TRUE(td_set_message(order));
}

// =============================================================================
// The root of the message and the children of a node
// =============================================================================

void test_no_message_has_no_root_and_no_name(void) {
    TEST_ASSERT_NULL(td_get_message_root());
    TEST_ASSERT_NULL(td_get_message_type_name());
}

void test_the_root_is_the_message(void) {
    build_message();
    TEST_ASSERT_NOT_NULL(td_get_message_root());
    TEST_ASSERT_EQUAL(VAL_STRUCT, td_get_message_root()->kind);
    TEST_ASSERT_EQUAL_STRING("Order", td_get_message_type_name());
}

// the domain is not the message
void test_the_domain_alone_gives_no_message(void) {
    s_domain_spec spec = {0};

    build_domain(&spec);
    TEST_ASSERT_NULL(td_get_message_root());
    TEST_ASSERT_NULL(td_get_message_type_name());
}

void test_children_are_given_in_order(void) {
    const s_struct_712_value *root;
    const s_struct_712_value *ids;

    build_message();
    root = td_get_message_root();
    TEST_ASSERT_EQUAL(2, td_value_child_count(root));
    TEST_ASSERT_EQUAL(VAL_ATOMIC, td_value_child(root, 0)->kind);
    ids = td_value_child(root, 1);
    TEST_ASSERT_EQUAL(VAL_ARRAY, ids->kind);
    TEST_ASSERT_EQUAL(3, td_value_child_count(ids));
    for (uint16_t i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL(i + 1, td_value_child(ids, i)->data[0]);
    }
}

void test_a_child_out_of_range_is_not_there(void) {
    const s_struct_712_value *root;

    build_message();
    root = td_get_message_root();
    TEST_ASSERT_NULL(td_value_child(root, 2));
    TEST_ASSERT_NULL(td_value_child(root, UINT16_MAX));
    TEST_ASSERT_NULL(td_value_child(td_value_child(root, 1), 3));
}

// a leaf holds data where a container holds its children: it has none
void test_a_leaf_and_nothing_have_no_children(void) {
    const s_struct_712_value *leaf;

    build_message();
    leaf = td_value_child(td_get_message_root(), 0);
    TEST_ASSERT_EQUAL(0, td_value_child_count(leaf));
    TEST_ASSERT_NULL(td_value_child(leaf, 0));
    TEST_ASSERT_EQUAL(0, td_value_child_count(NULL));
    TEST_ASSERT_NULL(td_value_child(NULL, 0));
}

// =============================================================================
// The chain of the domain
// =============================================================================

void test_no_domain_has_no_chain(void) {
    uint64_t chain_id = 0x1234;

    TEST_ASSERT_FALSE(td_get_domain_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(0x1234, chain_id);
}

void test_a_domain_without_chain_has_no_chain(void) {
    s_domain_spec spec = {0};
    uint64_t chain_id = 0x1234;

    build_domain(&spec);
    TEST_ASSERT_FALSE(td_get_domain_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(0x1234, chain_id);
}

void test_the_chain_is_read_big_endian(void) {
    static const uint8_t chain[] = {0x01, 0x02};
    s_domain_spec spec = {.has_chain_id = true,
                          .chain_id = chain,
                          .chain_id_length = sizeof(chain)};
    uint64_t chain_id = 0;

    build_domain(&spec);
    TEST_ASSERT_TRUE(td_get_domain_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(0x0102, chain_id);
}

void test_a_chain_of_eight_bytes_is_read_whole(void) {
    static const uint8_t chain[] = {0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xDB};
    s_domain_spec spec = {.has_chain_id = true,
                          .chain_id = chain,
                          .chain_id_length = sizeof(chain)};
    uint64_t chain_id = 0;

    build_domain(&spec);
    TEST_ASSERT_TRUE(td_get_domain_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(0x7FFFFFFFFFFFFFDBULL, chain_id);
}

// the hash of the domain commits to the full bytes: a padded value is the same number
void test_a_zero_padded_chain_is_the_same_number(void) {
    uint8_t chain[INT256_LENGTH] = {0};
    s_domain_spec spec = {.has_chain_id = true,
                          .chain_id = chain,
                          .chain_id_length = sizeof(chain)};
    uint64_t chain_id = 0;

    chain[INT256_LENGTH - 1] = 10;
    build_domain(&spec);
    TEST_ASSERT_TRUE(td_get_domain_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(10, chain_id);
}

// but a number that does not fit is not truncated into another one
void test_a_chain_that_does_not_fit_is_refused(void) {
    uint8_t chain[INT256_LENGTH] = {0};
    s_domain_spec spec = {.has_chain_id = true,
                          .chain_id = chain,
                          .chain_id_length = sizeof(chain)};
    uint64_t chain_id = 0x1234;

    chain[0] = 1;
    chain[INT256_LENGTH - 1] = 10;
    build_domain(&spec);
    TEST_ASSERT_FALSE(td_get_domain_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(0x1234, chain_id);
}

// the standard types chainId as an unsigned integer, of any size
void test_a_chain_of_a_smaller_unsigned_integer_is_a_chain(void) {
    static const uint8_t chain[] = {0x89};
    s_domain_spec spec = {.has_chain_id = true,
                          .chain_id_type = TYPE_SOL_UINT,
                          .chain_id = chain,
                          .chain_id_length = sizeof(chain)};
    uint64_t chain_id = 0;

    build_domain(&spec);
    TEST_ASSERT_TRUE(td_get_domain_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(0x89, chain_id);
}

// the bytes of any other type are not a number, even when they would read as one
void test_a_chain_that_is_not_an_unsigned_integer_is_refused(void) {
    static const uint8_t one[] = {0x01};
    static const uint8_t text[] = {'1'};
    static const uint8_t padded[INT256_LENGTH] = {[INT256_LENGTH - 1] = 0x01};
    static const uint8_t address[ADDRESS_LENGTH] = {[ADDRESS_LENGTH - 1] = 0x01};
    static const struct {
        e_type type;
        const uint8_t *value;
        size_t length;
    } cases[] = {
        {TYPE_SOL_STRING, text, sizeof(text)},
        {TYPE_SOL_INT, one, sizeof(one)},
        {TYPE_SOL_BOOL, one, sizeof(one)},
        {TYPE_SOL_BYTES_FIX, padded, sizeof(padded)},
        {TYPE_SOL_BYTES_DYN, one, sizeof(one)},
        {TYPE_SOL_ADDRESS, address, sizeof(address)},
    };

    for (size_t i = 0; i < ARRAYLEN(cases); ++i) {
        s_domain_spec spec = {.has_chain_id = true,
                              .chain_id_type = cases[i].type,
                              .chain_id = cases[i].value,
                              .chain_id_length = cases[i].length};
        uint64_t chain_id = 0x1234;

        td_deinit();
        build_domain(&spec);
        TEST_ASSERT_FALSE_MESSAGE(td_get_domain_chain_id(&chain_id), "type of chainId");
        TEST_ASSERT_EQUAL_UINT64(0x1234, chain_id);
    }
}

// =============================================================================
// The contract of the domain
// =============================================================================

static const uint8_t k_contract[ADDRESS_LENGTH] = {0xC0, 0x01, 0xC0, 0x02, 0xC0, 0x03, 0xC0,
                                                   0x04, 0xC0, 0x05, 0xC0, 0x06, 0xC0, 0x07,
                                                   0xC0, 0x08, 0xC0, 0x09, 0xC0, 0x0A};

void test_no_domain_has_no_contract(void) {
    uint8_t addr[ADDRESS_LENGTH];

    TEST_ASSERT_FALSE(td_get_domain_contract_addr(addr));
    TEST_ASSERT_FALSE(td_get_domain_verifying_address(addr));
}

void test_a_domain_without_contract_has_no_contract(void) {
    s_domain_spec spec = {0};
    uint8_t addr[ADDRESS_LENGTH];

    build_domain(&spec);
    TEST_ASSERT_FALSE(td_get_domain_contract_addr(addr));
    TEST_ASSERT_FALSE(td_get_domain_verifying_address(addr));
}

void test_an_address_is_the_contract(void) {
    s_domain_spec spec = {.has_contract = true,
                          .contract_type = TYPE_SOL_ADDRESS,
                          .contract = k_contract,
                          .contract_length = sizeof(k_contract)};
    uint8_t addr[ADDRESS_LENGTH] = {0};

    build_domain(&spec);
    TEST_ASSERT_TRUE(td_get_domain_contract_addr(addr));
    TEST_ASSERT_EQUAL_MEMORY(k_contract, addr, ADDRESS_LENGTH);
    memset(addr, 0, sizeof(addr));
    TEST_ASSERT_TRUE(td_get_domain_verifying_address(addr));
    TEST_ASSERT_EQUAL_MEMORY(k_contract, addr, ADDRESS_LENGTH);
}

void test_an_address_of_another_length_is_refused(void) {
    s_domain_spec spec = {.has_contract = true,
                          .contract_type = TYPE_SOL_ADDRESS,
                          .contract = k_contract,
                          .contract_length = ADDRESS_LENGTH - 1};
    uint8_t addr[ADDRESS_LENGTH];

    build_domain(&spec);
    TEST_ASSERT_FALSE(td_get_domain_contract_addr(addr));
    TEST_ASSERT_FALSE(td_get_domain_verifying_address(addr));
}

// Ethermint chains put a placeholder string there: it stands for an address when a contract is
// checked against the domain, but it is not one that can be called
void test_the_ethermint_placeholder_is_a_contract_to_check_but_not_to_call(void) {
    s_domain_spec spec = {.has_contract = true,
                          .contract_type = TYPE_SOL_STRING,
                          .contract = (const uint8_t *) "cosmos",
                          .contract_length = 6};
    uint8_t addr[ADDRESS_LENGTH];
    uint8_t expected[ADDRESS_LENGTH] = {'c', 'o', 's', 'm', 'o', 's'};

    build_domain(&spec);
    memset(addr, 0xFF, sizeof(addr));
    TEST_ASSERT_TRUE(td_get_domain_contract_addr(addr));
    TEST_ASSERT_EQUAL_MEMORY(expected, addr, ADDRESS_LENGTH);
    TEST_ASSERT_FALSE(td_get_domain_verifying_address(addr));
}

void test_another_string_is_not_a_contract(void) {
    s_domain_spec spec = {.has_contract = true,
                          .contract_type = TYPE_SOL_STRING,
                          .contract = (const uint8_t *) "cosmoz",
                          .contract_length = 6};
    uint8_t addr[ADDRESS_LENGTH];

    build_domain(&spec);
    TEST_ASSERT_FALSE(td_get_domain_contract_addr(addr));
    TEST_ASSERT_FALSE(td_get_domain_verifying_address(addr));
}

void test_the_placeholder_has_to_be_whole(void) {
    s_domain_spec spec = {.has_contract = true,
                          .contract_type = TYPE_SOL_STRING,
                          .contract = (const uint8_t *) "cosmos!",
                          .contract_length = 7};
    uint8_t addr[ADDRESS_LENGTH];

    build_domain(&spec);
    TEST_ASSERT_FALSE(td_get_domain_contract_addr(addr));
}

void test_a_number_is_not_a_contract(void) {
    s_domain_spec spec = {.has_contract = true,
                          .contract_type = TYPE_SOL_UINT,
                          .contract = k_contract,
                          .contract_length = ADDRESS_LENGTH};
    uint8_t addr[ADDRESS_LENGTH];

    build_domain(&spec);
    TEST_ASSERT_FALSE(td_get_domain_contract_addr(addr));
    TEST_ASSERT_FALSE(td_get_domain_verifying_address(addr));
}

// the chain and the contract of a domain are read where they are, whatever comes before them
void test_chain_and_contract_are_read_from_the_same_domain(void) {
    static const uint8_t chain[] = {0x89};
    s_domain_spec spec = {.has_chain_id = true,
                          .chain_id = chain,
                          .chain_id_length = sizeof(chain),
                          .has_contract = true,
                          .contract_type = TYPE_SOL_ADDRESS,
                          .contract = k_contract,
                          .contract_length = sizeof(k_contract)};
    uint64_t chain_id = 0;
    uint8_t addr[ADDRESS_LENGTH];

    build_domain(&spec);
    TEST_ASSERT_TRUE(td_get_domain_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(0x89, chain_id);
    TEST_ASSERT_TRUE(td_get_domain_verifying_address(addr));
    TEST_ASSERT_EQUAL_MEMORY(k_contract, addr, ADDRESS_LENGTH);
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
    RUN_TEST(test_no_message_has_no_root_and_no_name);
    RUN_TEST(test_the_root_is_the_message);
    RUN_TEST(test_the_domain_alone_gives_no_message);
    RUN_TEST(test_children_are_given_in_order);
    RUN_TEST(test_a_child_out_of_range_is_not_there);
    RUN_TEST(test_a_leaf_and_nothing_have_no_children);
    RUN_TEST(test_no_domain_has_no_chain);
    RUN_TEST(test_a_domain_without_chain_has_no_chain);
    RUN_TEST(test_the_chain_is_read_big_endian);
    RUN_TEST(test_a_chain_of_eight_bytes_is_read_whole);
    RUN_TEST(test_a_zero_padded_chain_is_the_same_number);
    RUN_TEST(test_a_chain_that_does_not_fit_is_refused);
    RUN_TEST(test_a_chain_of_a_smaller_unsigned_integer_is_a_chain);
    RUN_TEST(test_a_chain_that_is_not_an_unsigned_integer_is_refused);
    RUN_TEST(test_no_domain_has_no_contract);
    RUN_TEST(test_a_domain_without_contract_has_no_contract);
    RUN_TEST(test_an_address_is_the_contract);
    RUN_TEST(test_an_address_of_another_length_is_refused);
    RUN_TEST(test_the_ethermint_placeholder_is_a_contract_to_check_but_not_to_call);
    RUN_TEST(test_another_string_is_not_a_contract);
    RUN_TEST(test_the_placeholder_has_to_be_whole);
    RUN_TEST(test_a_number_is_not_a_contract);
    RUN_TEST(test_chain_and_contract_are_read_from_the_same_domain);
    return UNITY_END();
}
