/**
 * @file test_operation_ctx.c
 * @brief Unit tests for the GCS operation-context module at
 *        src/features/gcs/operation_ctx.c.
 *
 * operation_ctx tracks the per-operation state that survives across the
 * APDU stream: a doubly-linked list of s_operation_ctx nodes (one per outer
 * tx + optional inner sub-calls for proxy resolution), a typed message
 * kept apart from them as the root of the calls it makes, the
 * "current" pointer the parsers read through, and the parked
 * calldata buffer that hops from PROVIDE_CALLDATA into the node
 * created by operation_ctx_init().
 *
 * Tests focus on:
 *   - getter NULL-guards before any operation_ctx_init,
 *   - operation_ctx_init lifecycle: first call populates from/to/amount/
 *     chain_id, second call inherits from / chain_id from the
 *     previous tail, calldata ownership transfers (g_parked_calldata
 *     is cleared by the call),
 *   - operation_ctx_pop unlinks the current node and rewinds current,
 *   - find_matching_operation_ctx with selector + contract_addr + chain_id
 *     match,
 *   - validate_instruction_hash: real (mocked) hash compared against
 *     the stored fields_hash,
 *   - gcs_cleanup tears the whole state down,
 *   - a typed message as the root of its calls.
 *
 * Out of scope: process_empty_calls_before / process_empty_calls_after
 * pull in too many display-layer dependencies (ticker lookup,
 * amountToString, trusted-name resolution, getEthDisplayableAddress)
 * for the scope of this slice.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "operation_ctx.h"
#include "gcs_field_table.h"
#include "trusted_name.h"
#include "address_name_lookup.h"
#include "shared_context.h"
#include "status_words.h"

// =============================================================================
// Wraps / stubs for collaborators
// =============================================================================

// get_public_key writes ADDRESS_LENGTH bytes to its output buffer.
// We use a static "self" address for tests.
static const uint8_t g_self_addr[ADDRESS_LENGTH] = {
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
};

// process_empty_call is a static helper inside operation_ctx.c that drags display-
// layer symbols into the link even though our tests don't exercise it.
// Provide minimal stubs so the link resolves; these are NEVER called by
// the test cases below.

// Address-name resolution — reached only by the display path our tests skip.
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
    (void) addr;
    (void) chain_id;
    (void) type_count;
    (void) types;
    (void) source_count;
    (void) sources;
    (void) addr;
    // Mimic the production RAW fallback: always resolves, writing the first
    // address byte as display string and reporting the RAW source.
    if (buf != NULL && buf_size > 0) {
        snprintf(buf, buf_size, "%02x", addr[0]);
    }
    if (name_source_out != NULL) {
        *name_source_out = ADDR_NAME_FROM_RAW;
    }
    if (extra_data_out != NULL) {
        *extra_data_out = NULL;
    }
    return true;
}

// "To" rows added to the field table, in display order
#define MAX_TO_ROWS 4
static char g_to_rows[MAX_TO_ROWS][8];
static int g_to_rows_count = 0;
bool add_to_field_table(e_param_type type, const char *key, const char *value, const void *extra) {
    (void) type;
    (void) extra;
    if ((strcmp(key, "To") == 0) && (g_to_rows_count < MAX_TO_ROWS)) {
        strlcpy(g_to_rows[g_to_rows_count++], value, sizeof(g_to_rows[0]));
    }
    return true;
}

#define MAX_INTENT_ROWS 4
static uint8_t g_intent_batch_sizes[MAX_INTENT_ROWS];
static int g_intent_rows_count = 0;
bool set_intent_field(const char *value, uint8_t batch_size) {
    (void) value;
    if (g_intent_rows_count < MAX_INTENT_ROWS) {
        g_intent_batch_sizes[g_intent_rows_count++] = batch_size;
    }
    return true;
}

bool add_address_to_field_table(e_param_type type,
                                const char *key,
                                const char *value,
                                const void *extra,
                                const uint8_t *addr) {
    (void) addr;
    return add_to_field_table(type, key, value, extra);
}

static int g_network_calls = 0;
static uint64_t g_network_chain_id = 0;
bool get_network_as_string_from_chain_id(char *out, size_t out_size, uint64_t chain_id) {
    g_network_calls++;
    g_network_chain_id = chain_id;
    strlcpy(out, "Arbitrum", out_size);
    return true;
}

static bool g_public_key_ret = true;
uint16_t get_public_key(uint8_t *buf, uint8_t size) {
    if (!g_public_key_ret || (size != ADDRESS_LENGTH)) return SWO_INCORRECT_DATA;
    memcpy(buf, g_self_addr, ADDRESS_LENGTH);
    return SWO_SUCCESS;
}

// finalize_hash: tests set the bytes to inject via g_finalize_hash_out.
static uint8_t g_finalize_hash_out[INT256_LENGTH];
static bool g_finalize_hash_ret = true;
bool finalize_hash(cx_hash_t *hash_ctx, uint8_t *out, size_t out_len) {
    (void) hash_ctx;
    memcpy(out,
           g_finalize_hash_out,
           out_len < sizeof(g_finalize_hash_out) ? out_len : sizeof(g_finalize_hash_out));
    return g_finalize_hash_ret;
}

// operation_info.c is not linked in: same comparison as the real helper, driven by the
// finalize_hash stub above.
bool operation_info_fields_hash_is_empty(const s_operation_info *tx_info, bool *is_empty) {
    uint8_t hash[INT256_LENGTH];

    if (!finalize_hash(NULL, hash, sizeof(hash))) {
        return false;
    }
    *is_empty = (memcmp(hash, tx_info->fields_hash, sizeof(hash)) == 0);
    return true;
}

// ui / cleanup stubs
void ui_gcs_cleanup(void) {
}
static int g_delete_info_calls = 0;
void delete_operation_info(s_operation_info *node) {
    g_delete_info_calls++;
    free(node);
}
bool field_table_init(void) {
    return true;
}
void field_table_cleanup(void) {
}

// find_matching_operation_ctx uses get_implem_contract for proxy resolution.
static const uint8_t *g_implem_contract_ret = NULL;
const uint8_t *get_implem_contract(const uint64_t *chain_id,
                                   const uint8_t *to,
                                   const uint8_t *selector) {
    (void) chain_id;
    (void) to;
    (void) selector;
    return g_implem_contract_ret;
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    gcs_cleanup();
    appState = APP_STATE_IDLE;
    memset(g_finalize_hash_out, 0, sizeof(g_finalize_hash_out));
    g_finalize_hash_ret = true;
    g_implem_contract_ret = NULL;
    g_to_rows_count = 0;
    g_intent_rows_count = 0;
    g_public_key_ret = true;
    g_delete_info_calls = 0;
}

// Helper: set up a node and force g_operation_ctx_current to point at it by
// going through find_matching_operation_ctx. Returns the matched ctx through
// the getter side-effects.
static const uint8_t g_match_selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};

static s_calldata *make_complete_calldata(const uint8_t *selector) {
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, selector);
    uint8_t buf[CALLDATA_CHUNK_SIZE] = {0};
    calldata_append(cd, buf, CALLDATA_CHUNK_SIZE);
    return cd;
}

// Test data
static const uint8_t g_addr_a[ADDRESS_LENGTH] = {
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
};
static const uint8_t g_addr_b[ADDRESS_LENGTH] = {
    0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
    0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
};
static const uint8_t g_amount_1eth[INT256_LENGTH] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0,    0,    0,    0,    0,    0,    0,    0,
    0, 0, 0, 0, 0, 0, 0, 0, 0x0D, 0xE0, 0xB6, 0xB3, 0xA7, 0x64, 0x00, 0x00,
};

// =============================================================================
// Getters — NULL-guard behavior on an empty list
// =============================================================================

void test_getters_on_empty_list_return_null_or_zero(void) {
    uint64_t chain_id;
    s_operation_key key;

    TEST_ASSERT_FALSE(operation_ctx_is_root());
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 0);
    TEST_ASSERT_NULL(get_current_operation_info());
    TEST_ASSERT_NULL(get_root_operation_info());
    TEST_ASSERT_NULL(get_current_calldata());
    TEST_ASSERT_NULL(get_root_calldata());
    TEST_ASSERT_NULL(get_current_operation_from());
    TEST_ASSERT_NULL(get_current_operation_to());
    TEST_ASSERT_NULL(get_current_operation_amount());
    TEST_ASSERT_NULL(get_current_operation_fields_hash_ctx());
    TEST_ASSERT_FALSE(get_current_operation_chain_id(&chain_id));
    TEST_ASSERT_FALSE(get_current_operation_key(&key));
    TEST_ASSERT_FALSE(operation_ctx_is_describing_call());
    TEST_ASSERT_FALSE(operation_ctx_has_message());
}

// =============================================================================
// operation_ctx_init
// =============================================================================
// Note: operation_ctx_init pushes a new node but does NOT set g_operation_ctx_current.
// The current pointer is only set by find_matching_operation_ctx (or operation_ctx_pop
// rewinding to a previous current). So immediately after operation_ctx_init the
// getters still report empty even though the list has nodes.

void test_init_adds_to_list_but_current_stays_null(void) {
    uint64_t chain = 137;
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_b, g_amount_1eth, &chain));
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 1);
    // Current is not set yet — getters still NULL.
    TEST_ASSERT_FALSE(operation_ctx_is_root());
    TEST_ASSERT_NULL(get_current_operation_from());
    TEST_ASSERT_NULL(get_current_operation_to());
    TEST_ASSERT_NULL(get_current_operation_amount());
}

void test_init_then_find_matching_exposes_fields(void) {
    uint64_t chain = 137;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, g_amount_1eth, &chain));

    // Use find_matching_operation_ctx to set current onto the new node, which has the chain it was
    // given to be found by.
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));
    TEST_ASSERT_TRUE(operation_ctx_is_root());
    TEST_ASSERT_EQUAL_MEMORY(get_current_operation_from(), g_addr_a, ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_MEMORY(get_current_operation_to(), g_addr_b, ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_MEMORY(get_current_operation_amount(), g_amount_1eth, INT256_LENGTH);
}

void test_batch_size_is_kept_per_call(void) {
    uint64_t chain = 1;
    static const uint8_t selector2[CALLDATA_SELECTOR_SIZE] = {0xAA, 0xBB, 0xCC, 0xDD};

    TEST_ASSERT_FALSE(operation_ctx_set_last_batch_size(2));
    TEST_ASSERT_EQUAL(get_current_operation_batch_size(), 0);

    TEST_ASSERT_TRUE(operation_ctx_init(make_complete_calldata(g_match_selector),
                                        g_addr_a,
                                        g_addr_a,
                                        NULL,
                                        &chain));
    TEST_ASSERT_TRUE(
        operation_ctx_init(make_complete_calldata(selector2), NULL, g_addr_b, NULL, NULL));
    TEST_ASSERT_TRUE(operation_ctx_set_last_batch_size(3));

    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_a, g_match_selector, &chain));
    TEST_ASSERT_EQUAL(get_current_operation_batch_size(), 0);
    TEST_ASSERT_TRUE(operation_ctx_pop());
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, selector2, &chain));
    TEST_ASSERT_EQUAL(get_current_operation_batch_size(), 3);
}

void test_empty_call_intent_uses_its_own_batch_size(void) {
    uint64_t chain = 1;
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_set_last_batch_size(2));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_set_last_batch_size(1));
    TEST_ASSERT_TRUE(operation_ctx_init(make_complete_calldata(g_match_selector),
                                        g_addr_a,
                                        g_addr_b,
                                        NULL,
                                        &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));

    TEST_ASSERT_TRUE(process_empty_calls_before());
    TEST_ASSERT_EQUAL(g_intent_rows_count, 2);
    TEST_ASSERT_EQUAL(g_intent_batch_sizes[0], 2);
    TEST_ASSERT_EQUAL(g_intent_batch_sizes[1], 1);
}

void test_init_first_call_with_null_from_uses_self(void) {
    // get_public_key (wrapped) populates from when the caller passes NULL.
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, NULL, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));
    TEST_ASSERT_EQUAL_MEMORY(get_current_operation_from(), g_self_addr, ADDRESS_LENGTH);
}

void test_init_second_call_inherits_from_and_chain(void) {
    // First push sets from=addr_a, chain=56.
    uint64_t chain1 = 56;
    s_calldata *cd1 = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd1, g_addr_a, g_addr_a, NULL, &chain1));

    // Second push omits from / chain_id → must inherit from tail.
    // To then find_matching the second node we need a *different* selector
    // OR a different `to` since the matcher walks from head and returns
    // the first match.
    static const uint8_t selector2[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    s_calldata *cd2 = make_complete_calldata(selector2);
    TEST_ASSERT_TRUE(operation_ctx_init(cd2, NULL, g_addr_b, NULL, NULL));
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 2);
    // Execution-order provisioning: describe the root before the second
    // node is eligible for matching
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_a, g_match_selector, &chain1));
    s_operation_info *info = calloc(1, sizeof(*info));
    info->fields_hash[0] = 0xAA;  // mismatched hash: describe without popping
    TEST_ASSERT_TRUE(operation_ctx_set_info(info));
    // The second node's chain_id was inherited; use it for the match.
    uint64_t chain_lookup = 56;
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, selector2, &chain_lookup));
    TEST_ASSERT_EQUAL_MEMORY(get_current_operation_from(), g_addr_a, ADDRESS_LENGTH);
    // The matched node is the second one — not the root.
    TEST_ASSERT_FALSE(operation_ctx_is_root());
}

void test_init_clears_parked_calldata_pointer(void) {
    // Simulate the host having parked a calldata buffer.
    s_calldata *cd = make_complete_calldata(g_match_selector);
    g_parked_calldata = cd;
    uint64_t chain = 1;
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    // operation_ctx_init takes ownership and clears the global so the host
    // cannot double-free.
    TEST_ASSERT_NULL(g_parked_calldata);
}

// =============================================================================
// operation_ctx_pop
// =============================================================================

// a call that is not being described yet is not popped either
void test_pop_without_a_call_described_pops_nothing(void) {
    uint64_t chain = 1;

    TEST_ASSERT_FALSE(operation_ctx_pop());
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_FALSE(operation_ctx_pop());
    TEST_ASSERT_EQUAL(1, get_operation_ctx_count());
}

void test_pop_unlinks_current_node(void) {
    uint64_t chain1 = 1;
    s_calldata *cd1 = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd1, g_addr_a, g_addr_a, NULL, &chain1));

    static const uint8_t selector2[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    s_calldata *cd2 = make_complete_calldata(selector2);
    TEST_ASSERT_TRUE(operation_ctx_init(cd2, NULL, g_addr_b, NULL, NULL));
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 2);

    // Execution-order provisioning: describe the root first
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_a, g_match_selector, &chain1));
    s_operation_info *info = calloc(1, sizeof(*info));
    info->fields_hash[0] = 0xAA;  // mismatched hash: describe without popping
    TEST_ASSERT_TRUE(operation_ctx_set_info(info));

    // Match the second node to set current = tail.
    uint64_t chain_lookup = 1;
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, selector2, &chain_lookup));
    TEST_ASSERT_FALSE(operation_ctx_is_root());

    // Pop rewinds current to the previous node (the root).
    TEST_ASSERT_TRUE(operation_ctx_pop());
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 1);
    TEST_ASSERT_TRUE(operation_ctx_is_root());
    TEST_ASSERT_EQUAL_MEMORY(get_current_operation_from(), g_addr_a, ADDRESS_LENGTH);
}

// =============================================================================
// find_matching_operation_ctx
// =============================================================================

void test_find_matching_operation_ctx_happy(void) {
    // Two contexts at different addresses; only the second matches the
    // search.
    uint64_t chain = 1;
    static const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    s_calldata *cd_a = calldata_init(CALLDATA_CHUNK_SIZE, selector);
    s_calldata *cd_b = calldata_init(CALLDATA_CHUNK_SIZE, selector);
    // Mark both calldatas complete so calldata->selector is readable.
    uint8_t buf[CALLDATA_CHUNK_SIZE] = {0};
    calldata_append(cd_a, buf, CALLDATA_CHUNK_SIZE);
    calldata_append(cd_b, buf, CALLDATA_CHUNK_SIZE);

    TEST_ASSERT_TRUE(operation_ctx_init(cd_a, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_init(cd_b, NULL, g_addr_b, NULL, &chain));
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 2);

    // Only the next undescribed context in execution order is eligible:
    // describe the first node before matching the second
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_a, selector, &chain));
    s_operation_info *info = calloc(1, sizeof(*info));
    info->fields_hash[0] = 0xAA;  // mismatched hash: describe without popping
    TEST_ASSERT_TRUE(operation_ctx_set_info(info));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, selector, &chain));
    // current must now point at the second node — to=g_addr_b.
    TEST_ASSERT_EQUAL_MEMORY(get_current_operation_to(), g_addr_b, ADDRESS_LENGTH);
}

void test_find_matching_operation_ctx_no_match_keeps_current(void) {
    uint64_t chain = 1;
    static const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, selector);
    uint8_t buf[CALLDATA_CHUNK_SIZE] = {0};
    calldata_append(cd, buf, CALLDATA_CHUNK_SIZE);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_a, NULL, &chain));

    // Search for an unrelated address.
    TEST_ASSERT_FALSE(find_matching_operation_ctx(g_addr_b, selector, &chain));
}

// =============================================================================
// validate_instruction_hash
// =============================================================================

void test_validate_instruction_hash_matches(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));

    s_operation_info *heap_info = malloc(sizeof(s_operation_info));
    memset(heap_info, 0, sizeof(*heap_info));
    heap_info->fields_hash[0] = 0x11;
    heap_info->fields_hash[31] = 0xFF;

    // operation_ctx_set_info only attaches the info to the call being described.
    TEST_ASSERT_TRUE(operation_ctx_set_info(heap_info));

    // Now make finalize_hash return the exact fields_hash so the
    // validator returns true.
    memcpy(g_finalize_hash_out, heap_info->fields_hash, sizeof(heap_info->fields_hash));
    TEST_ASSERT_TRUE(validate_instruction_hash());
}

void test_validate_instruction_hash_mismatch(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));

    s_operation_info *heap_info = malloc(sizeof(s_operation_info));
    memset(heap_info, 0, sizeof(*heap_info));
    heap_info->fields_hash[0] = 0xFE;
    TEST_ASSERT_TRUE(operation_ctx_set_info(heap_info));

    memset(g_finalize_hash_out, 0xCC, sizeof(g_finalize_hash_out));
    TEST_ASSERT_FALSE(validate_instruction_hash());
}

void test_validate_instruction_hash_no_current_returns_false(void) {
    TEST_ASSERT_FALSE(validate_instruction_hash());
}

// =============================================================================
// gcs_cleanup
// =============================================================================

void test_gcs_cleanup_empties_list(void) {
    uint64_t chain = 1;
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, NULL, g_addr_b, NULL, NULL));
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 2);
    gcs_cleanup();
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 0);
    TEST_ASSERT_NULL(g_parked_calldata);
}

void test_gcs_cleanup_frees_parked_calldata(void) {
    g_parked_calldata = calldata_init(0, NULL);
    gcs_cleanup();
    TEST_ASSERT_NULL(g_parked_calldata);
}

// The shown flags belong to the calls, which a typed message releases without gcs_cleanup()
void test_operation_ctx_cleanup_forgets_what_the_descriptors_showed(void) {
    operation_ctx_set_root_from_shown();
    operation_ctx_set_root_value_shown();
    TEST_ASSERT_TRUE(operation_ctx_is_root_from_shown());
    TEST_ASSERT_TRUE(operation_ctx_is_root_value_shown());
    operation_ctx_cleanup();
    TEST_ASSERT_FALSE(operation_ctx_is_root_from_shown());
    TEST_ASSERT_FALSE(operation_ctx_is_root_value_shown());
}

// =============================================================================
// process_empty_calls_before / process_empty_calls_after
// =============================================================================
//
// An "empty" tx is a operation_ctx with no calldata attached — the parser
// inserts these as placeholders when the host first announces a batch
// of related transactions, then the active "with calldata" operation_ctx is
// pushed afterwards. process_empty_calls_{before,after} walks the
// neighbours of the current pointer, calls process_empty_call on each
// empty one (which set_intent_field / amount / trusted_name / address
// formatting via the stubs above), and removes them from the list.

// Empty predecessors are shown in execution order, not walking backward
void test_process_empty_calls_before_keeps_execution_order(void) {
    static const uint8_t to_1[ADDRESS_LENGTH] = {0x01};
    static const uint8_t to_2[ADDRESS_LENGTH] = {0x02};
    static const uint8_t to_3[ADDRESS_LENGTH] = {0x03};
    uint64_t chain = 1;
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, to_1, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, to_2, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, to_3, NULL, &chain));
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));

    TEST_ASSERT_TRUE(process_empty_calls_before());
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 1);
    TEST_ASSERT_EQUAL(g_to_rows_count, 3);
    TEST_ASSERT_EQUAL_STRING("01", g_to_rows[0]);
    TEST_ASSERT_EQUAL_STRING("02", g_to_rows[1]);
    TEST_ASSERT_EQUAL_STRING("03", g_to_rows[2]);
}

void test_process_empty_calls_before_removes_empty_predecessors(void) {
    uint64_t chain = 1;
    // 1) Two empty (calldata == NULL) predecessors
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    // 2) The "with calldata" current tx
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 3);

    TEST_ASSERT_TRUE(process_empty_calls_before());
    // Both empty predecessors must be gone — only the current one
    // remains.
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 1);
}

void test_process_empty_calls_after_removes_empty_successors(void) {
    uint64_t chain = 1;
    // 1) Current with calldata
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    // 2) Two empty successors
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 3);

    TEST_ASSERT_TRUE(process_empty_calls_after());
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 1);
}

void test_process_empty_calls_before_stops_at_non_empty(void) {
    uint64_t chain = 1;
    // Layout: [empty A] [withCalldata B] [current C, withCalldata]
    // process_empty_calls_before walks current -> prev. The first prev
    // (B) has calldata so the loop exits immediately. A is NOT
    // processed (still has calldata != NULL — wait, A is empty).
    // The loop reads tmp->calldata == NULL as the gate; B has
    // calldata so loop stops at B without touching A.
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));  // A empty
    s_calldata *cd_b = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(
        operation_ctx_init(cd_b, g_addr_a, g_addr_a, NULL, &chain));  // B with calldata
    static const uint8_t selector_c[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    s_calldata *cd_c = make_complete_calldata(selector_c);
    TEST_ASSERT_TRUE(operation_ctx_init(cd_c, g_addr_a, g_addr_b, NULL, &chain));  // C current
    // A is empty (skipped by the matcher); B must be described before C is
    // eligible, in execution order
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_a, g_match_selector, &chain));
    s_operation_info *info_b = calloc(1, sizeof(*info_b));
    info_b->fields_hash[0] = 0xAA;  // mismatched hash: describe without popping
    TEST_ASSERT_TRUE(operation_ctx_set_info(info_b));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, selector_c, &chain));
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 3);

    TEST_ASSERT_TRUE(process_empty_calls_before());
    // B blocks the walk → A stays untouched.
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 3);
}

void test_process_empty_calls_with_amount_no_tx_info_uses_nested_chain(void) {
    uint64_t chain = 1;
    // Empty predecessor with has_amount=true drives process_empty_call down
    // the "Send" branch. The nested context carries its own chain ID, so no
    // tx_info (node or root) is needed.
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &chain));
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));

    TEST_ASSERT_TRUE(process_empty_calls_before());
    // The empty node was processed and removed
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 1);
}

void test_process_empty_call_cross_chain_shows_network(void) {
    uint64_t root_chain = 1;
    uint64_t nested_chain = 42161;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &root_chain));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &nested_chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &root_chain));

    g_network_calls = 0;
    TEST_ASSERT_TRUE(process_empty_calls_after());
    TEST_ASSERT_EQUAL(1, g_network_calls);
    TEST_ASSERT_EQUAL_UINT64(nested_chain, g_network_chain_id);
}

void test_process_empty_call_same_chain_no_network(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));

    g_network_calls = 0;
    TEST_ASSERT_TRUE(process_empty_calls_after());
    TEST_ASSERT_EQUAL(0, g_network_calls);
}

// =============================================================================
// Remaining gaps: simple getters with current set, process_empty_call
// amount happy path, operation_ctx_init field_table_init path.
// =============================================================================

void test_get_fields_hash_ctx_returns_current_ctx(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));
    // Non-NULL is enough — the pointer references the current node's
    // fields_hash_ctx member, opaque from here.
    TEST_ASSERT_NOT_NULL(get_current_operation_fields_hash_ctx());
}

void test_get_current_getters_return_attached_state(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, g_amount_1eth, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));

    // tx_info attached after the fact via operation_ctx_set_info.
    s_operation_info *info = calloc(1, sizeof(*info));
    TEST_ASSERT_TRUE(operation_ctx_set_info(info));

    TEST_ASSERT_EQUAL_PTR(get_current_operation_info(), info);
    TEST_ASSERT_EQUAL_PTR(get_root_operation_info(), info);  // root == current here
    TEST_ASSERT_NOT_NULL(get_current_calldata());
    TEST_ASSERT_NOT_NULL(get_root_calldata());
}

// Amount-branch happy path inside process_empty_call. Tags an empty
// predecessor with an amount, attaches a tx_info to the root (so
// get_root_operation_info finds one), and lets the trusted_name / amount /
// add_to_field_table stubs sail through.
void test_process_empty_calls_amount_branch_happy_path(void) {
    uint64_t chain = 1;
    s_calldata *cd_root = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd_root, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));

    // Attach tx_info to the root so process_empty_call's tx_info==NULL ->
    // get_root_operation_info() resolution path succeeds.
    s_operation_info *info = calloc(1, sizeof(*info));
    info->chain_id = 1;
    TEST_ASSERT_TRUE(operation_ctx_set_info(info));

    // Insert an empty SUCCESSOR with has_amount=true (g_amount_1eth).
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &chain));
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 2);

    // process_empty_calls_after now drives process_empty_call's amount path:
    //   set_intent_field("Send") + get_root_operation_info + amountToString +
    //   add_to_field_table + trusted_name lookup (returns NULL, falls
    //   back to RAW + getEthDisplayableAddress) + final add_to_field_table.
    TEST_ASSERT_TRUE(process_empty_calls_after());
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 1);  // empty successor removed
}

// operation_ctx_init under APP_STATE_SIGNING_TX takes the field_table_init
// branch at the end (line 312-313). The wrapped field_table_init
// returns true, so the init succeeds.
void test_operation_ctx_init_signing_tx_calls_field_table_init(void) {
    uint64_t chain = 1;
    appState = APP_STATE_SIGNING_TX;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_EQUAL(get_operation_ctx_count(), 1);
    // Restore — gcs_cleanup in reset() will run between tests.
    appState = APP_STATE_IDLE;
}

// =============================================================================
// A typed message
// =============================================================================

// a message info, which announces descriptors when asked to: its fields hash is not the one of
// none
static s_operation_info *make_message_info(bool announces_fields) {
    s_operation_info *info = calloc(1, sizeof(*info));

    info->context = OPERATION_INFO_CTX_EIP712;
    memset(info->domain_separator, 0xD5, sizeof(info->domain_separator));
    memset(info->primary_type_hash, 0x5A, sizeof(info->primary_type_hash));
    if (announces_fields) {
        info->fields_hash[0] = 0xAA;
    }
    return info;
}

// the message is the root of the operations: the first operation, and the one being described
void test_a_message_is_the_root_and_the_operation_being_described(void) {
    s_operation_info *info = make_message_info(false);
    uint64_t chain_id = 0;

    TEST_ASSERT_TRUE(operation_ctx_init_message(info, g_addr_b, g_addr_a, &(uint64_t) {137}));
    TEST_ASSERT_TRUE(operation_ctx_has_message());
    TEST_ASSERT_TRUE(operation_ctx_is_root());
    TEST_ASSERT_EQUAL_PTR(info, get_current_operation_info());
    TEST_ASSERT_EQUAL_PTR(info, get_root_operation_info());
    // it is not a call
    TEST_ASSERT_FALSE(operation_ctx_is_describing_call());
    TEST_ASSERT_EQUAL(0, get_operation_ctx_count());
    TEST_ASSERT_NULL(get_root_calldata());
    // what it is evaluated against: its signer, its contract, its chain, no amount
    TEST_ASSERT_EQUAL_MEMORY(g_addr_b, get_current_operation_from(), ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_MEMORY(g_addr_a, get_current_operation_to(), ADDRESS_LENGTH);
    TEST_ASSERT_TRUE(get_current_operation_chain_id(&chain_id));
    TEST_ASSERT_EQUAL_UINT64(137, chain_id);
    TEST_ASSERT_NULL(get_current_operation_amount());
    TEST_ASSERT_NOT_NULL(get_current_operation_fields_hash_ctx());
}

void test_a_message_has_no_callee_when_its_domain_has_no_contract(void) {
    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(false), g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_NULL(get_current_operation_to());
}

// a message whose domain has no chain is on none
void test_a_message_with_no_chain_is_on_none(void) {
    uint64_t chain_id = 0;

    TEST_ASSERT_TRUE(operation_ctx_init_message(make_message_info(false), g_self_addr, NULL, NULL));
    TEST_ASSERT_FALSE(get_current_operation_chain_id(&chain_id));
}

// the metadata of a message is attached to its primary type in its domain, the one of a call to its
// function of its callee: a message needs no chain nor contract for it
void test_the_key_of_a_message_is_its_domain_and_primary_type(void) {
    s_operation_key key;

    TEST_ASSERT_TRUE(operation_ctx_init_message(make_message_info(false), g_self_addr, NULL, NULL));
    TEST_ASSERT_TRUE(get_current_operation_key(&key));
    TEST_ASSERT_EQUAL(OPERATION_INFO_CTX_EIP712, key.context);
    TEST_ASSERT_EACH_EQUAL_UINT8(0xD5, key.domain_separator, INT256_LENGTH);
    TEST_ASSERT_EACH_EQUAL_UINT8(0x5A, key.primary_type_hash, INT256_LENGTH);
}

void test_the_key_of_a_call_is_its_chain_callee_and_selector(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    s_operation_info *info = calloc(1, sizeof(*info));
    s_operation_key key;

    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));
    info->fields_hash[0] = 0xAA;
    TEST_ASSERT_TRUE(operation_ctx_set_info(info));
    TEST_ASSERT_TRUE(get_current_operation_key(&key));
    TEST_ASSERT_EQUAL(OPERATION_INFO_CTX_SIGN_TX, key.context);
    TEST_ASSERT_EQUAL_UINT64(1, key.chain_id);
    TEST_ASSERT_EQUAL_MEMORY(g_addr_b, key.contract_addr, ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_MEMORY(g_match_selector, key.selector, CALLDATA_SELECTOR_SIZE);
}

// a call stays a call, whatever the info it is described with
void test_a_call_is_not_taken_for_a_message_by_its_info(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);

    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));
    TEST_ASSERT_TRUE(operation_ctx_set_info(make_message_info(true)));
    TEST_ASSERT_FALSE(operation_ctx_has_message());
    TEST_ASSERT_TRUE(operation_ctx_is_describing_call());
    TEST_ASSERT_EQUAL(1, get_operation_ctx_count());
}

// the message is only started first, and by an info that can be told what it announces
void test_a_message_that_cannot_be_started_is_refused_and_its_info_released(void) {
    g_finalize_hash_ret = false;
    TEST_ASSERT_FALSE(
        operation_ctx_init_message(make_message_info(false), g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_FALSE(operation_ctx_has_message());
    TEST_ASSERT_EQUAL(1, g_delete_info_calls);
    TEST_ASSERT_NULL(get_current_operation_info());
}

void test_a_message_cannot_come_after_a_call(void) {
    uint64_t chain = 1;

    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_FALSE(
        operation_ctx_init_message(make_message_info(false), g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_FALSE(operation_ctx_has_message());
    TEST_ASSERT_EQUAL(1, g_delete_info_calls);
    TEST_ASSERT_EQUAL(1, get_operation_ctx_count());
}

void test_a_message_cannot_come_after_a_message(void) {
    s_operation_info *first = make_message_info(true);

    TEST_ASSERT_TRUE(operation_ctx_init_message(first, g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_FALSE(
        operation_ctx_init_message(make_message_info(false), g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_EQUAL(1, g_delete_info_calls);
    TEST_ASSERT_EQUAL_PTR(first, get_current_operation_info());
    TEST_ASSERT_TRUE(operation_ctx_message_has_pending_fields());
}

// An info only goes to a call that is being described: not to the typed message, nor to nothing,
// and the caller keeps the info then
void test_set_info_is_refused_when_no_call_is_being_described(void) {
    s_operation_info *info = calloc(1, sizeof(*info));

    TEST_ASSERT_FALSE(operation_ctx_set_info(info));
    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_FALSE(operation_ctx_set_info(info));
    TEST_ASSERT_NOT_EQUAL(info, get_current_operation_info());
    TEST_ASSERT_EQUAL(0, g_delete_info_calls);
    free(info);
}

void test_cleanup_forgets_the_message(void) {
    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, &(uint64_t) {1}));
    operation_ctx_cleanup();
    TEST_ASSERT_FALSE(operation_ctx_has_message());
    TEST_ASSERT_FALSE(operation_ctx_message_has_pending_fields());
    TEST_ASSERT_NULL(get_current_operation_info());
    TEST_ASSERT_EQUAL(1, g_delete_info_calls);
    // and a new flow may have its own
    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(false), g_self_addr, NULL, &(uint64_t) {1}));
}

// =============================================================================
// The descriptors of a message, and the ones its info announces
// =============================================================================

void test_descriptors_are_pending_when_the_info_announces_some(void) {
    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_TRUE(operation_ctx_message_has_pending_fields());
}

void test_no_descriptors_are_pending_when_the_info_announces_none(void) {
    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(false), g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_FALSE(operation_ctx_message_has_pending_fields());
    TEST_ASSERT_FALSE(operation_ctx_message_fields_match());
}

void test_no_descriptors_are_pending_without_a_message(void) {
    TEST_ASSERT_FALSE(operation_ctx_message_has_pending_fields());
    TEST_ASSERT_FALSE(operation_ctx_message_fields_match());
}

// the announcement is the hash over every descriptor, in the order they came
void test_descriptors_match_when_the_digest_is_the_announced_one(void) {
    s_operation_info *info = make_message_info(true);

    memset(info->fields_hash, 0x5A, sizeof(info->fields_hash));
    TEST_ASSERT_TRUE(operation_ctx_init_message(info, g_self_addr, NULL, &(uint64_t) {1}));
    memset(g_finalize_hash_out, 0x5A, sizeof(g_finalize_hash_out));
    TEST_ASSERT_TRUE(operation_ctx_message_fields_match());
}

void test_descriptors_do_not_match_another_digest(void) {
    s_operation_info *info = make_message_info(true);

    memset(info->fields_hash, 0x5A, sizeof(info->fields_hash));
    TEST_ASSERT_TRUE(operation_ctx_init_message(info, g_self_addr, NULL, &(uint64_t) {1}));
    memset(g_finalize_hash_out, 0x5A, sizeof(g_finalize_hash_out));
    // the last byte is enough
    g_finalize_hash_out[INT256_LENGTH - 1] ^= 0x01;
    TEST_ASSERT_FALSE(operation_ctx_message_fields_match());
    g_finalize_hash_out[INT256_LENGTH - 1] ^= 0x01;
    g_finalize_hash_out[0] ^= 0x01;
    TEST_ASSERT_FALSE(operation_ctx_message_fields_match());
}

void test_descriptors_do_not_match_when_the_hash_cannot_be_finalized(void) {
    s_operation_info *info = make_message_info(true);

    memset(info->fields_hash, 0x5A, sizeof(info->fields_hash));
    TEST_ASSERT_TRUE(operation_ctx_init_message(info, g_self_addr, NULL, &(uint64_t) {1}));
    memset(g_finalize_hash_out, 0x5A, sizeof(g_finalize_hash_out));
    g_finalize_hash_ret = false;
    TEST_ASSERT_FALSE(operation_ctx_message_fields_match());
}

// =============================================================================
// The calls a typed message makes
// =============================================================================

// the calls of a message are not its own: they are counted apart, and are not the root
void test_a_call_of_a_message_is_not_the_message(void) {
    uint64_t chain = 137;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    s_operation_info *message = make_message_info(true);

    TEST_ASSERT_TRUE(operation_ctx_init_message(message, g_self_addr, NULL, &(uint64_t) {137}));
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_b, g_addr_a, NULL, &chain));
    TEST_ASSERT_EQUAL(1, get_operation_ctx_count());
    TEST_ASSERT_TRUE(operation_ctx_has_message());
    // the call is not being described until a description is matched to it
    TEST_ASSERT_FALSE(operation_ctx_is_describing_call());
    TEST_ASSERT_EQUAL_PTR(message, get_current_operation_info());
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_a, g_match_selector, &chain));
    TEST_ASSERT_TRUE(operation_ctx_is_describing_call());
    TEST_ASSERT_FALSE(operation_ctx_is_root());
    TEST_ASSERT_NULL(get_current_operation_info());
    TEST_ASSERT_EQUAL_MEMORY(g_addr_a, get_current_operation_to(), ADDRESS_LENGTH);
}

// their sender and their chain are the message's, when they do not say
void test_init_in_a_typed_message_takes_its_sender_and_chain(void) {
    s_calldata *cd = make_complete_calldata(g_match_selector);
    uint64_t chain = 137;

    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, &(uint64_t) {137}));
    TEST_ASSERT_TRUE(operation_ctx_init(cd, NULL, g_addr_a, NULL, NULL));
    // the chain of the call is the one it is found by
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_a, g_match_selector, &chain));
    TEST_ASSERT_EQUAL_MEMORY(g_self_addr, get_current_operation_from(), ADDRESS_LENGTH);
}

void test_init_in_a_typed_message_keeps_what_the_descriptor_says(void) {
    s_calldata *cd = make_complete_calldata(g_match_selector);
    uint64_t chain = 10;

    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, &(uint64_t) {137}));
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_a, g_match_selector, &chain));
    TEST_ASSERT_EQUAL_MEMORY(g_addr_a, get_current_operation_from(), ADDRESS_LENGTH);
}

// no chain is not a chain: no description matches a call on it
// a call that takes no chain from its message is matched by no info, as an info always has one
void test_init_in_a_typed_message_without_chain_gives_no_chain(void) {
    s_calldata *cd = make_complete_calldata(g_match_selector);
    uint64_t chain = 1;

    TEST_ASSERT_TRUE(operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, NULL));
    TEST_ASSERT_TRUE(operation_ctx_init(cd, NULL, g_addr_a, NULL, NULL));
    TEST_ASSERT_FALSE(find_matching_operation_ctx(g_addr_a, g_match_selector, &chain));
    chain = 0;
    TEST_ASSERT_FALSE(find_matching_operation_ctx(g_addr_a, g_match_selector, &chain));
}

// the calls named by the message are not the calls of one another
void test_init_of_another_call_of_a_typed_message_does_not_take_the_previous_one(void) {
    static const uint8_t selector2[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    s_calldata *cd1 = make_complete_calldata(g_match_selector);
    s_calldata *cd2 = make_complete_calldata(selector2);
    uint64_t chain1 = 10;
    uint64_t chain2 = 137;

    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, &(uint64_t) {137}));
    TEST_ASSERT_TRUE(operation_ctx_init(cd1, g_addr_a, g_addr_a, NULL, &chain1));
    TEST_ASSERT_TRUE(operation_ctx_init(cd2, NULL, g_addr_a, NULL, NULL));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_a, g_match_selector, &chain1));
    s_operation_info *info = calloc(1, sizeof(*info));
    info->fields_hash[0] = 0xAA;  // not the hash of the descriptors: stays described
    TEST_ASSERT_TRUE(operation_ctx_set_info(info));
    // the second was created while the message, not the first call, was described
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_a, selector2, &chain2));
    TEST_ASSERT_EQUAL_MEMORY(g_self_addr, get_current_operation_from(), ADDRESS_LENGTH);
}

// a call made by a call is made by it
void test_init_in_a_call_of_a_typed_message_takes_the_call(void) {
    s_calldata *cd1 = make_complete_calldata(g_match_selector);
    s_calldata *cd2 = make_complete_calldata(g_match_selector);
    uint64_t chain1 = 10;

    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, &(uint64_t) {137}));
    TEST_ASSERT_TRUE(operation_ctx_init(cd1, g_addr_a, g_addr_a, NULL, &chain1));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_a, g_match_selector, &chain1));
    TEST_ASSERT_TRUE(operation_ctx_init(cd2, NULL, g_addr_b, NULL, NULL));
    TEST_ASSERT_EQUAL(2, get_operation_ctx_count());
    // it was queued with the sender of the one being described, and its chain: it is found by it
    TEST_ASSERT_EQUAL_MEMORY(g_addr_a, get_current_operation_from(), ADDRESS_LENGTH);
    s_operation_info *info = calloc(1, sizeof(*info));
    info->fields_hash[0] = 0xAA;
    TEST_ASSERT_TRUE(operation_ctx_set_info(info));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain1));
}

void test_nothing_is_being_described_until_a_description_is_matched(void) {
    s_calldata *cd = make_complete_calldata(g_match_selector);
    uint64_t chain = 1;

    TEST_ASSERT_FALSE(operation_ctx_is_describing_call());
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_FALSE(operation_ctx_is_describing_call());
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));
    TEST_ASSERT_TRUE(operation_ctx_is_describing_call());
}

// a call of a message is popped when its descriptors are complete, and the message is the one
// described again
void test_pop_of_a_call_of_a_typed_message_describes_the_message_again(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    s_operation_info *info = calloc(1, sizeof(*info));
    s_operation_info *message = make_message_info(true);

    TEST_ASSERT_TRUE(operation_ctx_init_message(message, g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));
    TEST_ASSERT_FALSE(operation_ctx_is_root());
    TEST_ASSERT_TRUE(operation_ctx_set_info(info));
    TEST_ASSERT_TRUE(operation_ctx_pop());
    TEST_ASSERT_EQUAL(0, get_operation_ctx_count());
    TEST_ASSERT_TRUE(operation_ctx_has_message());
    TEST_ASSERT_FALSE(operation_ctx_is_describing_call());
    TEST_ASSERT_TRUE(operation_ctx_is_root());
    TEST_ASSERT_EQUAL_PTR(message, get_current_operation_info());
}

// and a call that was made after another one is described again once it is popped
void test_pop_of_a_nested_call_of_a_typed_message_describes_the_call_before(void) {
    static const uint8_t selector2[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    s_calldata *cd1 = make_complete_calldata(g_match_selector);
    s_calldata *cd2 = make_complete_calldata(selector2);
    uint64_t chain = 1;
    s_operation_info *info = calloc(1, sizeof(*info));

    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_TRUE(operation_ctx_init(cd1, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_init(cd2, NULL, g_addr_b, NULL, NULL));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_a, g_match_selector, &chain));
    info->fields_hash[0] = 0xAA;
    TEST_ASSERT_TRUE(operation_ctx_set_info(info));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, selector2, &chain));
    TEST_ASSERT_TRUE(operation_ctx_pop());
    TEST_ASSERT_EQUAL(1, get_operation_ctx_count());
    TEST_ASSERT_TRUE(operation_ctx_is_describing_call());
    TEST_ASSERT_EQUAL_PTR(info, get_current_operation_info());
}

// the message is not a call, so it is never popped
void test_pop_leaves_the_message(void) {
    s_operation_info *message = make_message_info(true);

    TEST_ASSERT_TRUE(operation_ctx_init_message(message, g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_FALSE(operation_ctx_pop());
    TEST_ASSERT_TRUE(operation_ctx_has_message());
    TEST_ASSERT_EQUAL_PTR(message, get_current_operation_info());
    TEST_ASSERT_EQUAL(0, g_delete_info_calls);
}

// the message is the root of the calls, not an empty call to be shown from the call that follows
void test_the_message_is_not_taken_for_an_empty_call(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);

    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_operation_ctx(g_addr_b, g_match_selector, &chain));
    g_to_rows_count = 0;
    TEST_ASSERT_TRUE(process_empty_calls_before());
    TEST_ASSERT_EQUAL(0, g_to_rows_count);
    TEST_ASSERT_TRUE(operation_ctx_has_message());
    TEST_ASSERT_EQUAL(1, get_operation_ctx_count());
}

// a call on another chain than the message's is told apart by its network
void test_empty_call_of_a_typed_message_on_another_chain_shows_its_network(void) {
    uint64_t other_chain = 10;

    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &other_chain));
    g_network_calls = 0;
    TEST_ASSERT_TRUE(process_remaining_empty_calls());
    TEST_ASSERT_EQUAL(1, g_network_calls);
}

void test_empty_call_of_a_typed_message_on_its_chain_shows_no_network(void) {
    uint64_t chain = 1;

    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &chain));
    g_network_calls = 0;
    TEST_ASSERT_TRUE(process_remaining_empty_calls());
    TEST_ASSERT_EQUAL(0, g_network_calls);
}

// a message that is on no chain has no chain to share: a call on its own one is told apart
void test_empty_call_of_a_typed_message_with_no_chain_shows_its_network(void) {
    uint64_t chain = 1;

    TEST_ASSERT_TRUE(operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, NULL));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &chain));
    g_network_calls = 0;
    TEST_ASSERT_TRUE(process_remaining_empty_calls());
    TEST_ASSERT_EQUAL(1, g_network_calls);
}

// and a call that takes no chain from it goes nowhere that can be shown
void test_empty_call_on_no_chain_is_refused(void) {
    TEST_ASSERT_TRUE(operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, NULL));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, NULL));
    g_network_calls = 0;
    g_to_rows_count = 0;
    TEST_ASSERT_FALSE(process_remaining_empty_calls());
    TEST_ASSERT_EQUAL(0, g_network_calls);
    TEST_ASSERT_EQUAL(0, g_to_rows_count);
}

// what is left when no description is to come: calls that have nothing to be described
void test_remaining_empty_calls_are_all_processed(void) {
    uint64_t chain = 1;

    TEST_ASSERT_TRUE(process_remaining_empty_calls());
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &chain));
    TEST_ASSERT_EQUAL(2, get_operation_ctx_count());
    TEST_ASSERT_TRUE(process_remaining_empty_calls());
    TEST_ASSERT_EQUAL(0, get_operation_ctx_count());
}

// the message stays, as the root of the calls that were processed
void test_remaining_empty_calls_of_a_typed_message_leave_the_message(void) {
    uint64_t chain = 1;

    TEST_ASSERT_TRUE(
        operation_ctx_init_message(make_message_info(true), g_self_addr, NULL, &(uint64_t) {1}));
    TEST_ASSERT_TRUE(process_remaining_empty_calls());
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &chain));
    TEST_ASSERT_EQUAL(2, get_operation_ctx_count());
    TEST_ASSERT_TRUE(process_remaining_empty_calls());
    TEST_ASSERT_EQUAL(0, get_operation_ctx_count());
    TEST_ASSERT_TRUE(operation_ctx_has_message());
}

void test_remaining_calls_with_calldata_are_not_processed(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);

    TEST_ASSERT_TRUE(operation_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(operation_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_FALSE(process_remaining_empty_calls());
    // the empty one before it was, and the call that waits for its description is still there
    TEST_ASSERT_EQUAL(1, get_operation_ctx_count());
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
    RUN_TEST(test_getters_on_empty_list_return_null_or_zero);
    RUN_TEST(test_init_adds_to_list_but_current_stays_null);
    RUN_TEST(test_init_then_find_matching_exposes_fields);
    RUN_TEST(test_batch_size_is_kept_per_call);
    RUN_TEST(test_empty_call_intent_uses_its_own_batch_size);
    RUN_TEST(test_init_first_call_with_null_from_uses_self);
    RUN_TEST(test_init_second_call_inherits_from_and_chain);
    RUN_TEST(test_init_clears_parked_calldata_pointer);
    RUN_TEST(test_pop_without_a_call_described_pops_nothing);
    RUN_TEST(test_pop_unlinks_current_node);
    RUN_TEST(test_find_matching_operation_ctx_happy);
    RUN_TEST(test_find_matching_operation_ctx_no_match_keeps_current);
    RUN_TEST(test_validate_instruction_hash_matches);
    RUN_TEST(test_validate_instruction_hash_mismatch);
    RUN_TEST(test_validate_instruction_hash_no_current_returns_false);
    RUN_TEST(test_gcs_cleanup_empties_list);
    RUN_TEST(test_gcs_cleanup_frees_parked_calldata);
    RUN_TEST(test_operation_ctx_cleanup_forgets_what_the_descriptors_showed);
    RUN_TEST(test_process_empty_calls_before_removes_empty_predecessors);
    RUN_TEST(test_process_empty_calls_before_keeps_execution_order);
    RUN_TEST(test_process_empty_calls_after_removes_empty_successors);
    RUN_TEST(test_process_empty_calls_before_stops_at_non_empty);
    RUN_TEST(test_process_empty_calls_with_amount_no_tx_info_uses_nested_chain);
    RUN_TEST(test_process_empty_call_cross_chain_shows_network);
    RUN_TEST(test_process_empty_call_same_chain_no_network);
    RUN_TEST(test_get_fields_hash_ctx_returns_current_ctx);
    RUN_TEST(test_get_current_getters_return_attached_state);
    RUN_TEST(test_process_empty_calls_amount_branch_happy_path);
    RUN_TEST(test_operation_ctx_init_signing_tx_calls_field_table_init);
    RUN_TEST(test_a_message_is_the_root_and_the_operation_being_described);
    RUN_TEST(test_a_message_has_no_callee_when_its_domain_has_no_contract);
    RUN_TEST(test_a_message_with_no_chain_is_on_none);
    RUN_TEST(test_the_key_of_a_message_is_its_domain_and_primary_type);
    RUN_TEST(test_the_key_of_a_call_is_its_chain_callee_and_selector);
    RUN_TEST(test_a_call_is_not_taken_for_a_message_by_its_info);
    RUN_TEST(test_a_message_that_cannot_be_started_is_refused_and_its_info_released);
    RUN_TEST(test_a_message_cannot_come_after_a_call);
    RUN_TEST(test_a_message_cannot_come_after_a_message);
    RUN_TEST(test_set_info_is_refused_when_no_call_is_being_described);
    RUN_TEST(test_cleanup_forgets_the_message);
    RUN_TEST(test_descriptors_are_pending_when_the_info_announces_some);
    RUN_TEST(test_no_descriptors_are_pending_when_the_info_announces_none);
    RUN_TEST(test_no_descriptors_are_pending_without_a_message);
    RUN_TEST(test_descriptors_match_when_the_digest_is_the_announced_one);
    RUN_TEST(test_descriptors_do_not_match_another_digest);
    RUN_TEST(test_descriptors_do_not_match_when_the_hash_cannot_be_finalized);
    RUN_TEST(test_a_call_of_a_message_is_not_the_message);
    RUN_TEST(test_init_in_a_typed_message_takes_its_sender_and_chain);
    RUN_TEST(test_init_in_a_typed_message_keeps_what_the_descriptor_says);
    RUN_TEST(test_init_in_a_typed_message_without_chain_gives_no_chain);
    RUN_TEST(test_init_of_another_call_of_a_typed_message_does_not_take_the_previous_one);
    RUN_TEST(test_init_in_a_call_of_a_typed_message_takes_the_call);
    RUN_TEST(test_nothing_is_being_described_until_a_description_is_matched);
    RUN_TEST(test_pop_of_a_call_of_a_typed_message_describes_the_message_again);
    RUN_TEST(test_pop_of_a_nested_call_of_a_typed_message_describes_the_call_before);
    RUN_TEST(test_pop_leaves_the_message);
    RUN_TEST(test_the_message_is_not_taken_for_an_empty_call);
    RUN_TEST(test_empty_call_of_a_typed_message_on_another_chain_shows_its_network);
    RUN_TEST(test_empty_call_of_a_typed_message_on_its_chain_shows_no_network);
    RUN_TEST(test_empty_call_of_a_typed_message_with_no_chain_shows_its_network);
    RUN_TEST(test_empty_call_on_no_chain_is_refused);
    RUN_TEST(test_remaining_empty_calls_are_all_processed);
    RUN_TEST(test_remaining_empty_calls_of_a_typed_message_leave_the_message);
    RUN_TEST(test_remaining_calls_with_calldata_are_not_processed);
    return UNITY_END();
}
