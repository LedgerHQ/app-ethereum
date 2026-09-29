/**
 * @file test_tx_ctx.c
 * @brief Unit tests for the GCS transaction-context module at
 *        src/features/generic_tx_parser/tx_ctx.c.
 *
 * tx_ctx tracks the per-transaction state that survives across the
 * APDU stream: a doubly-linked list of s_tx_ctx nodes (one per outer
 * tx + optional inner sub-calls for proxy resolution), the
 * "current" pointer the parsers read through, and the parked
 * calldata buffer that hops from PROVIDE_CALLDATA into the node
 * created by tx_ctx_init().
 *
 * Tests focus on:
 *   - getter NULL-guards before any tx_ctx_init,
 *   - tx_ctx_init lifecycle: first call populates from/to/amount/
 *     chain_id, second call inherits from / chain_id from the
 *     previous tail, calldata ownership transfers (g_parked_calldata
 *     is cleared by the call),
 *   - tx_ctx_pop unlinks the current node and rewinds current,
 *   - find_matching_tx_ctx with selector + contract_addr + chain_id
 *     match,
 *   - validate_instruction_hash: real (mocked) hash compared against
 *     the stored fields_hash,
 *   - gcs_cleanup tears the whole state down.
 *
 * Out of scope: process_empty_txs_before / process_empty_txs_after
 * pull in too many display-layer dependencies (ticker lookup,
 * amountToString, trusted-name resolution, getEthDisplayableAddress)
 * for the scope of this slice.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "tx_ctx.h"
#include "gtp_field_table.h"
#include "trusted_name.h"
#include "address_name_lookup.h"
#include "shared_context.h"

// =============================================================================
// Globals
// =============================================================================

// =============================================================================
// Wraps / stubs for collaborators
// =============================================================================

// get_public_key writes ADDRESS_LENGTH bytes to its output buffer.
// We use a static "self" address for tests.
static const uint8_t g_self_addr[ADDRESS_LENGTH] = {
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
};

// process_empty_tx is a static helper inside tx_ctx.c that drags display-
// layer symbols into the link even though our tests don't exercise it.
// Provide minimal stubs so the link resolves; these are NEVER called by
// the test cases below.

// EIP-712 calldata-info helpers — only reached when appState ==
// APP_STATE_SIGNING_EIP712, which our tests never set.
void *get_current_calldata_info(void) {
    return NULL;
}
bool calldata_info_all_received(const void *info) {
    (void) info;
    return false;
}
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
    // Mimic the production RAW fallback: always resolves, writing a
    // placeholder display string and reporting the RAW source.
    if (buf != NULL && buf_size > 0) {
        buf[0] = '\0';
    }
    if (name_source_out != NULL) {
        *name_source_out = ADDR_NAME_FROM_RAW;
    }
    if (extra_data_out != NULL) {
        *extra_data_out = NULL;
    }
    return true;
}

static int g_network_calls = 0;
static uint64_t g_network_chain_id = 0;
bool get_network_as_string_from_chain_id(char *out, size_t out_size, uint64_t chain_id) {
    g_network_calls++;
    g_network_chain_id = chain_id;
    strlcpy(out, "Arbitrum", out_size);
    return true;
}

bool get_public_key(uint8_t *buf, uint8_t size) {
    if (size != ADDRESS_LENGTH) return false;
    memcpy(buf, g_self_addr, ADDRESS_LENGTH);
    return true;
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

// ui / cleanup stubs
void ui_gcs_cleanup(void) {
}
void delete_tx_info(s_tx_info *node) {
    free(node);
}
bool field_table_init(void) {
    return true;
}
void field_table_cleanup(void) {
}

// find_matching_tx_ctx uses get_implem_contract for proxy resolution.
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
}

// Helper: set up a node and force g_tx_ctx_current to point at it by
// going through find_matching_tx_ctx. Returns the matched ctx through
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
    TEST_ASSERT_FALSE(tx_ctx_is_root());
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 0);
    TEST_ASSERT_NULL(get_current_tx_info());
    TEST_ASSERT_NULL(get_root_tx_info());
    TEST_ASSERT_NULL(get_current_calldata());
    TEST_ASSERT_NULL(get_root_calldata());
    TEST_ASSERT_NULL(get_current_tx_from());
    TEST_ASSERT_NULL(get_current_tx_to());
    TEST_ASSERT_NULL(get_current_tx_amount());
    TEST_ASSERT_EQUAL(get_current_tx_chain_id(), 0);
}

// =============================================================================
// tx_ctx_init
// =============================================================================
// Note: tx_ctx_init pushes a new node but does NOT set g_tx_ctx_current.
// The current pointer is only set by find_matching_tx_ctx (or tx_ctx_pop
// rewinding to a previous current). So immediately after tx_ctx_init the
// getters still report empty even though the list has nodes.

void test_init_adds_to_list_but_current_stays_null(void) {
    uint64_t chain = 137;
    TEST_ASSERT_TRUE(tx_ctx_init(NULL, g_addr_a, g_addr_b, g_amount_1eth, &chain));
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 1);
    // Current is not set yet — getters still NULL.
    TEST_ASSERT_FALSE(tx_ctx_is_root());
    TEST_ASSERT_NULL(get_current_tx_from());
    TEST_ASSERT_NULL(get_current_tx_to());
    TEST_ASSERT_NULL(get_current_tx_amount());
    TEST_ASSERT_EQUAL(get_current_tx_chain_id(), 0);
}

void test_init_then_find_matching_exposes_fields(void) {
    uint64_t chain = 137;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, g_amount_1eth, &chain));

    // Use find_matching_tx_ctx to set current onto the new node.
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &chain));
    TEST_ASSERT_TRUE(tx_ctx_is_root());
    TEST_ASSERT_EQUAL_MEMORY(get_current_tx_from(), g_addr_a, ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_MEMORY(get_current_tx_to(), g_addr_b, ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_MEMORY(get_current_tx_amount(), g_amount_1eth, INT256_LENGTH);
    TEST_ASSERT_EQUAL(get_current_tx_chain_id(), 137);
}

void test_init_first_call_with_null_from_uses_self(void) {
    // get_public_key (wrapped) populates from when the caller passes NULL.
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, NULL, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &chain));
    TEST_ASSERT_EQUAL_MEMORY(get_current_tx_from(), g_self_addr, ADDRESS_LENGTH);
}

void test_init_second_call_inherits_from_and_chain(void) {
    // First push sets from=addr_a, chain=56.
    uint64_t chain1 = 56;
    s_calldata *cd1 = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd1, g_addr_a, g_addr_a, NULL, &chain1));

    // Second push omits from / chain_id → must inherit from tail.
    // To then find_matching the second node we need a *different* selector
    // OR a different `to` since the matcher walks from head and returns
    // the first match.
    static const uint8_t selector2[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    s_calldata *cd2 = make_complete_calldata(selector2);
    TEST_ASSERT_TRUE(tx_ctx_init(cd2, NULL, g_addr_b, NULL, NULL));
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 2);
    // Execution-order provisioning: describe the root before the second
    // node is eligible for matching
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_a, g_match_selector, &chain1));
    s_tx_info *info = calloc(1, sizeof(*info));
    info->fields_hash[0] = 0xAA;  // mismatched hash: describe without popping
    TEST_ASSERT_TRUE(set_tx_info_into_tx_ctx(info));
    // The second node's chain_id was inherited; use it for the match.
    uint64_t chain_lookup = 56;
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, selector2, &chain_lookup));
    TEST_ASSERT_EQUAL_MEMORY(get_current_tx_from(), g_addr_a, ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL(get_current_tx_chain_id(), 56);
    // The matched node is the second one — not the root.
    TEST_ASSERT_FALSE(tx_ctx_is_root());
}

void test_init_clears_parked_calldata_pointer(void) {
    // Simulate the host having parked a calldata buffer.
    s_calldata *cd = make_complete_calldata(g_match_selector);
    g_parked_calldata = cd;
    uint64_t chain = 1;
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    // tx_ctx_init takes ownership and clears the global so the host
    // cannot double-free.
    TEST_ASSERT_NULL(g_parked_calldata);
}

// =============================================================================
// tx_ctx_pop
// =============================================================================

void test_pop_unlinks_current_node(void) {
    uint64_t chain1 = 1;
    s_calldata *cd1 = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd1, g_addr_a, g_addr_a, NULL, &chain1));

    static const uint8_t selector2[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    s_calldata *cd2 = make_complete_calldata(selector2);
    TEST_ASSERT_TRUE(tx_ctx_init(cd2, NULL, g_addr_b, NULL, NULL));
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 2);

    // Execution-order provisioning: describe the root first
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_a, g_match_selector, &chain1));
    s_tx_info *info = calloc(1, sizeof(*info));
    info->fields_hash[0] = 0xAA;  // mismatched hash: describe without popping
    TEST_ASSERT_TRUE(set_tx_info_into_tx_ctx(info));

    // Match the second node to set current = tail.
    uint64_t chain_lookup = 1;
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, selector2, &chain_lookup));
    TEST_ASSERT_FALSE(tx_ctx_is_root());

    // Pop rewinds current to the previous node (the root).
    tx_ctx_pop();
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 1);
    TEST_ASSERT_TRUE(tx_ctx_is_root());
    TEST_ASSERT_EQUAL_MEMORY(get_current_tx_from(), g_addr_a, ADDRESS_LENGTH);
}

// =============================================================================
// find_matching_tx_ctx
// =============================================================================

void test_find_matching_tx_ctx_happy(void) {
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

    TEST_ASSERT_TRUE(tx_ctx_init(cd_a, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(tx_ctx_init(cd_b, NULL, g_addr_b, NULL, &chain));
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 2);

    // Only the next undescribed context in execution order is eligible:
    // describe the first node before matching the second
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_a, selector, &chain));
    s_tx_info *info = calloc(1, sizeof(*info));
    info->fields_hash[0] = 0xAA;  // mismatched hash: describe without popping
    TEST_ASSERT_TRUE(set_tx_info_into_tx_ctx(info));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, selector, &chain));
    // current must now point at the second node — to=g_addr_b.
    TEST_ASSERT_EQUAL_MEMORY(get_current_tx_to(), g_addr_b, ADDRESS_LENGTH);
}

void test_find_matching_tx_ctx_no_match_keeps_current(void) {
    uint64_t chain = 1;
    static const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, selector);
    uint8_t buf[CALLDATA_CHUNK_SIZE] = {0};
    calldata_append(cd, buf, CALLDATA_CHUNK_SIZE);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_a, NULL, &chain));

    // Search for an unrelated address.
    TEST_ASSERT_FALSE(find_matching_tx_ctx(g_addr_b, selector, &chain));
}

// =============================================================================
// validate_instruction_hash
// =============================================================================

void test_validate_instruction_hash_matches(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &chain));

    s_tx_info *heap_info = malloc(sizeof(s_tx_info));
    memset(heap_info, 0, sizeof(*heap_info));
    heap_info->fields_hash[0] = 0x11;
    heap_info->fields_hash[31] = 0xFF;

    // set_tx_info_into_tx_ctx at root with appState=IDLE skips both the
    // intent-field write and the auto-pop, so it only attaches the info.
    appState = APP_STATE_IDLE;
    TEST_ASSERT_TRUE(set_tx_info_into_tx_ctx(heap_info));

    // Now make finalize_hash return the exact fields_hash so the
    // validator returns true.
    memcpy(g_finalize_hash_out, heap_info->fields_hash, sizeof(heap_info->fields_hash));
    TEST_ASSERT_TRUE(validate_instruction_hash());
}

void test_validate_instruction_hash_mismatch(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &chain));

    s_tx_info *heap_info = malloc(sizeof(s_tx_info));
    memset(heap_info, 0, sizeof(*heap_info));
    heap_info->fields_hash[0] = 0xFE;
    appState = APP_STATE_IDLE;
    TEST_ASSERT_TRUE(set_tx_info_into_tx_ctx(heap_info));

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
    TEST_ASSERT_TRUE(tx_ctx_init(NULL, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(tx_ctx_init(NULL, NULL, g_addr_b, NULL, NULL));
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 2);
    gcs_cleanup();
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 0);
    TEST_ASSERT_NULL(g_parked_calldata);
}

void test_gcs_cleanup_frees_parked_calldata(void) {
    g_parked_calldata = calldata_init(0, NULL);
    gcs_cleanup();
    TEST_ASSERT_NULL(g_parked_calldata);
}

// =============================================================================
// process_empty_txs_before / process_empty_txs_after
// =============================================================================
//
// An "empty" tx is a tx_ctx with no calldata attached — the parser
// inserts these as placeholders when the host first announces a batch
// of related transactions, then the active "with calldata" tx_ctx is
// pushed afterwards. process_empty_txs_{before,after} walks the
// neighbours of the current pointer, calls process_empty_tx on each
// empty one (which set_intent_field / amount / trusted_name / address
// formatting via the stubs above), and removes them from the list.

void test_process_empty_txs_before_removes_empty_predecessors(void) {
    uint64_t chain = 1;
    // 1) Two empty (calldata == NULL) predecessors
    TEST_ASSERT_TRUE(tx_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(tx_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    // 2) The "with calldata" current tx
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &chain));
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 3);

    TEST_ASSERT_TRUE(process_empty_txs_before());
    // Both empty predecessors must be gone — only the current one
    // remains.
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 1);
}

void test_process_empty_txs_after_removes_empty_successors(void) {
    uint64_t chain = 1;
    // 1) Current with calldata
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    // 2) Two empty successors
    TEST_ASSERT_TRUE(tx_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(tx_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &chain));
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 3);

    TEST_ASSERT_TRUE(process_empty_txs_after());
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 1);
}

void test_process_empty_txs_before_stops_at_non_empty(void) {
    uint64_t chain = 1;
    // Layout: [empty A] [withCalldata B] [current C, withCalldata]
    // process_empty_txs_before walks current -> prev. The first prev
    // (B) has calldata so the loop exits immediately. A is NOT
    // processed (still has calldata != NULL — wait, A is empty).
    // The loop reads tmp->calldata == NULL as the gate; B has
    // calldata so loop stops at B without touching A.
    TEST_ASSERT_TRUE(tx_ctx_init(NULL, g_addr_a, g_addr_a, NULL, &chain));  // A empty
    s_calldata *cd_b = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd_b, g_addr_a, g_addr_a, NULL, &chain));  // B with calldata
    static const uint8_t selector_c[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    s_calldata *cd_c = make_complete_calldata(selector_c);
    TEST_ASSERT_TRUE(tx_ctx_init(cd_c, g_addr_a, g_addr_b, NULL, &chain));  // C current
    // A is empty (skipped by the matcher); B must be described before C is
    // eligible, in execution order
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_a, g_match_selector, &chain));
    s_tx_info *info_b = calloc(1, sizeof(*info_b));
    info_b->fields_hash[0] = 0xAA;  // mismatched hash: describe without popping
    TEST_ASSERT_TRUE(set_tx_info_into_tx_ctx(info_b));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, selector_c, &chain));
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 3);

    TEST_ASSERT_TRUE(process_empty_txs_before());
    // B blocks the walk → A stays untouched.
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 3);
}

void test_process_empty_txs_with_amount_no_tx_info_uses_nested_chain(void) {
    uint64_t chain = 1;
    // Empty predecessor with has_amount=true drives process_empty_tx down
    // the "Send" branch. The nested context carries its own chain ID, so no
    // tx_info (node or root) is needed.
    TEST_ASSERT_TRUE(tx_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &chain));
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &chain));

    TEST_ASSERT_TRUE(process_empty_txs_before());
    // The empty node was processed and removed
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 1);
}

void test_process_empty_tx_cross_chain_shows_network(void) {
    uint64_t root_chain = 1;
    uint64_t nested_chain = 42161;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, NULL, &root_chain));
    TEST_ASSERT_TRUE(tx_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &nested_chain));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &root_chain));

    g_network_calls = 0;
    TEST_ASSERT_TRUE(process_empty_txs_after());
    TEST_ASSERT_EQUAL(1, g_network_calls);
    TEST_ASSERT_EQUAL_UINT64(nested_chain, g_network_chain_id);
}

void test_process_empty_tx_same_chain_no_network(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(tx_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &chain));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &chain));

    g_network_calls = 0;
    TEST_ASSERT_TRUE(process_empty_txs_after());
    TEST_ASSERT_EQUAL(0, g_network_calls);
}

// =============================================================================
// Remaining gaps: simple getters with current set, process_empty_tx
// amount happy path, set_tx_info_into_tx_ctx EIP712 + non-root,
// tx_ctx_init field_table_init path.
// =============================================================================

void test_get_fields_hash_ctx_returns_current_ctx(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &chain));
    // Non-NULL is enough — the pointer references the current node's
    // fields_hash_ctx member, opaque from here.
    TEST_ASSERT_NOT_NULL(get_fields_hash_ctx());
}

void test_get_current_getters_return_attached_state(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, g_amount_1eth, &chain));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &chain));

    // tx_info attached after the fact via set_tx_info_into_tx_ctx.
    s_tx_info *info = calloc(1, sizeof(*info));
    appState = APP_STATE_IDLE;
    TEST_ASSERT_TRUE(set_tx_info_into_tx_ctx(info));

    TEST_ASSERT_EQUAL_PTR(get_current_tx_info(), info);
    TEST_ASSERT_EQUAL_PTR(get_root_tx_info(), info);  // root == current here
    TEST_ASSERT_NOT_NULL(get_current_calldata());
    TEST_ASSERT_NOT_NULL(get_root_calldata());
}

// Amount-branch happy path inside process_empty_tx. Tags an empty
// predecessor with an amount, attaches a tx_info to the root (so
// get_root_tx_info finds one), and lets the trusted_name / amount /
// add_to_field_table stubs sail through.
void test_process_empty_txs_amount_branch_happy_path(void) {
    uint64_t chain = 1;
    s_calldata *cd_root = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd_root, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &chain));

    // Attach tx_info to the root so process_empty_tx's tx_info==NULL ->
    // get_root_tx_info() resolution path succeeds.
    s_tx_info *info = calloc(1, sizeof(*info));
    info->chain_id = 1;
    appState = APP_STATE_IDLE;
    TEST_ASSERT_TRUE(set_tx_info_into_tx_ctx(info));

    // Insert an empty SUCCESSOR with has_amount=true (g_amount_1eth).
    TEST_ASSERT_TRUE(tx_ctx_init(NULL, g_addr_a, g_addr_a, g_amount_1eth, &chain));
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 2);

    // process_empty_txs_after now drives process_empty_tx's amount path:
    //   set_intent_field("Send") + get_root_tx_info + amountToString +
    //   add_to_field_table + trusted_name lookup (returns NULL, falls
    //   back to RAW + getEthDisplayableAddress) + final add_to_field_table.
    TEST_ASSERT_TRUE(process_empty_txs_after());
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 1);  // empty successor removed
}

// set_tx_info_into_tx_ctx with appState=SIGNING_EIP712 takes the
// EIP712 path: set_intent_field + cx_sha3_init + finalize_hash. We
// arrange the hash mock so it does NOT match the tx_info hash, so
// tx_ctx_pop is skipped (matching pop would otherwise leave the
// node un-attached and surprise the assertion).
void test_set_tx_info_eip712_at_root_runs_hash_path(void) {
    uint64_t chain = 1;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, g_match_selector, &chain));

    s_tx_info *info = calloc(1, sizeof(*info));
    info->fields_hash[0] = 0xAA;  // distinct from g_finalize_hash_out
    appState = APP_STATE_SIGNING_EIP712;
    TEST_ASSERT_TRUE(set_tx_info_into_tx_ctx(info));
    // Node should still be attached (no pop) since hash mismatched.
    TEST_ASSERT_EQUAL_PTR(get_current_tx_info(), info);
}

// set_tx_info_into_tx_ctx on a non-root node (g_tx_ctx_current is the
// second node). The hash path runs unconditionally, set_intent_field
// is called outside the EIP712 branch.
void test_set_tx_info_non_root_runs_intent_and_hash(void) {
    uint64_t chain = 1;
    // Root
    s_calldata *cd_root = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd_root, g_addr_a, g_addr_a, NULL, &chain));
    // Second node (will become current)
    static const uint8_t selector_2[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    s_calldata *cd2 = make_complete_calldata(selector_2);
    TEST_ASSERT_TRUE(tx_ctx_init(cd2, g_addr_a, g_addr_b, NULL, &chain));
    // Execution-order provisioning: describe the root first
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_a, g_match_selector, &chain));
    s_tx_info *info_root = calloc(1, sizeof(*info_root));
    info_root->fields_hash[0] = 0xAA;  // mismatched hash: describe without popping
    TEST_ASSERT_TRUE(set_tx_info_into_tx_ctx(info_root));
    TEST_ASSERT_TRUE(find_matching_tx_ctx(g_addr_b, selector_2, &chain));
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 2);

    s_tx_info *info = calloc(1, sizeof(*info));
    info->fields_hash[0] = 0xAA;
    appState = APP_STATE_IDLE;
    // Non-root path: set_intent_field called, hash path runs, hash
    // mismatches so no pop. Node stays.
    TEST_ASSERT_TRUE(set_tx_info_into_tx_ctx(info));
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 2);
}

// tx_ctx_init under APP_STATE_SIGNING_TX takes the field_table_init
// branch at the end (line 312-313). The wrapped field_table_init
// returns true, so the init succeeds.
void test_tx_ctx_init_signing_tx_calls_field_table_init(void) {
    uint64_t chain = 1;
    appState = APP_STATE_SIGNING_TX;
    s_calldata *cd = make_complete_calldata(g_match_selector);
    TEST_ASSERT_TRUE(tx_ctx_init(cd, g_addr_a, g_addr_b, NULL, &chain));
    TEST_ASSERT_EQUAL(get_tx_ctx_count(), 1);
    // Restore — gcs_cleanup in reset() will run between tests.
    appState = APP_STATE_IDLE;
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
    RUN_TEST(test_init_first_call_with_null_from_uses_self);
    RUN_TEST(test_init_second_call_inherits_from_and_chain);
    RUN_TEST(test_init_clears_parked_calldata_pointer);
    RUN_TEST(test_pop_unlinks_current_node);
    RUN_TEST(test_find_matching_tx_ctx_happy);
    RUN_TEST(test_find_matching_tx_ctx_no_match_keeps_current);
    RUN_TEST(test_validate_instruction_hash_matches);
    RUN_TEST(test_validate_instruction_hash_mismatch);
    RUN_TEST(test_validate_instruction_hash_no_current_returns_false);
    RUN_TEST(test_gcs_cleanup_empties_list);
    RUN_TEST(test_gcs_cleanup_frees_parked_calldata);
    RUN_TEST(test_process_empty_txs_before_removes_empty_predecessors);
    RUN_TEST(test_process_empty_txs_after_removes_empty_successors);
    RUN_TEST(test_process_empty_txs_before_stops_at_non_empty);
    RUN_TEST(test_process_empty_txs_with_amount_no_tx_info_uses_nested_chain);
    RUN_TEST(test_process_empty_tx_cross_chain_shows_network);
    RUN_TEST(test_process_empty_tx_same_chain_no_network);
    RUN_TEST(test_get_fields_hash_ctx_returns_current_ctx);
    RUN_TEST(test_get_current_getters_return_attached_state);
    RUN_TEST(test_process_empty_txs_amount_branch_happy_path);
    RUN_TEST(test_set_tx_info_eip712_at_root_runs_hash_path);
    RUN_TEST(test_set_tx_info_non_root_runs_intent_and_hash);
    RUN_TEST(test_tx_ctx_init_signing_tx_calls_field_table_init);
    return UNITY_END();
}
