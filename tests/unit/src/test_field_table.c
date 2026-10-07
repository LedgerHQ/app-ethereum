/**
 * @file test_field_table.c
 * @brief Unit tests for the GCS field table at
 *        src/features/generic_tx_parser/gtp_field_table.c.
 *
 * The field table is the in-memory buffer of (type, key, value,
 * extra_data) entries the parser hands to the UI layer to render a
 * transaction. Every gtp_param_* formatter ends in a call to
 * add_to_field_table(), so this module sits squarely on the critical
 * path for what the user sees.
 *
 * Behaviors covered:
 *   - field_table_init / cleanup lifecycle (and the dirty-init rejection
 *     that catches a forgotten cleanup),
 *   - add_to_field_table NULL-pointer guards,
 *   - happy-path append: heap-allocated copies of key/value, type
 *     stored verbatim for most entries, end_intent flag fed from
 *     validate_instruction_hash,
 *   - special types: PARAM_TYPE_INTENT and PARAM_TYPE_SEPARATOR both
 *     rewrite the stored type to PARAM_TYPE_RAW after raising the
 *     corresponding flag,
 *   - EIP-712 short-circuit: when appState == APP_STATE_SIGNING_EIP712
 *     the call routes through ui_712_set_* helpers instead of growing
 *     the table,
 *   - set_intent_field rewrites the type based on
 *     txContext.current_batch_size,
 *   - get_from_field_table returns entries in insertion order.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_field_table.h"
#include "gtp_field.h"
#include "shared_context.h"
#include "tx_ctx.h"

// =============================================================================
// Globals the module reads
// =============================================================================

// =============================================================================
// Stubs for collaborators
// =============================================================================

static bool g_validate_hash_ret = false;
bool validate_instruction_hash(void) {
    return g_validate_hash_ret;
}

static int g_ui_712_intent_calls = 0;
static int g_ui_712_title_calls = 0;
static int g_ui_712_value_calls = 0;
static char g_ui_712_last_title[64] = {0};
static char g_ui_712_last_value[64] = {0};

bool ui_712_set_intent(void) {
    g_ui_712_intent_calls++;
    return true;
}

bool ui_712_set_title(const char *str, size_t length) {
    g_ui_712_title_calls++;
    size_t copy =
        length < sizeof(g_ui_712_last_title) - 1 ? length : sizeof(g_ui_712_last_title) - 1;
    memcpy(g_ui_712_last_title, str, copy);
    g_ui_712_last_title[copy] = '\0';
    return true;
}

bool ui_712_set_value(const char *str, size_t length) {
    g_ui_712_value_calls++;
    size_t copy =
        length < sizeof(g_ui_712_last_value) - 1 ? length : sizeof(g_ui_712_last_value) - 1;
    memcpy(g_ui_712_last_value, str, copy);
    g_ui_712_last_value[copy] = '\0';
    return true;
}

// =============================================================================
// Fixture: every test starts with an empty table
// =============================================================================

static void reset(void) {
    field_table_cleanup();
    appState = APP_STATE_IDLE;
    memset(&txContext, 0, sizeof(txContext));
    g_validate_hash_ret = false;
    g_ui_712_intent_calls = 0;
    g_ui_712_title_calls = 0;
    g_ui_712_value_calls = 0;
    g_ui_712_last_title[0] = '\0';
    g_ui_712_last_value[0] = '\0';
}

// =============================================================================
// field_table_init lifecycle
// =============================================================================

void test_init_on_clean_succeeds(void) {
    TEST_ASSERT_TRUE(field_table_init());
    TEST_ASSERT_EQUAL(field_table_size(), 0);
}

void test_init_when_dirty_cleans_and_rejects(void) {
    TEST_ASSERT_TRUE(add_to_field_table(PARAM_TYPE_RAW, "k", "v", NULL));
    TEST_ASSERT_EQUAL(field_table_size(), 1);
    // Init on a non-empty table must reject AND clean up (next call to
    // size confirms the table is empty).
    TEST_ASSERT_FALSE(field_table_init());
    TEST_ASSERT_EQUAL(field_table_size(), 0);
}

// =============================================================================
// add_to_field_table — guards
// =============================================================================

void test_add_null_key_rejected(void) {
    TEST_ASSERT_FALSE(add_to_field_table(PARAM_TYPE_RAW, NULL, "v", NULL));
    TEST_ASSERT_EQUAL(field_table_size(), 0);
}

void test_add_null_value_rejected(void) {
    TEST_ASSERT_FALSE(add_to_field_table(PARAM_TYPE_RAW, "k", NULL, NULL));
    TEST_ASSERT_EQUAL(field_table_size(), 0);
}

// =============================================================================
// add_to_field_table — happy paths
// =============================================================================

void test_add_stores_copies_and_type(void) {
    int sentinel = 42;
    TEST_ASSERT_TRUE(add_to_field_table(PARAM_TYPE_AMOUNT, "Amount", "1 ETH", &sentinel));
    TEST_ASSERT_EQUAL(field_table_size(), 1);

    const s_field_table_entry *e = get_from_field_table(0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL(e->type, PARAM_TYPE_AMOUNT);
    TEST_ASSERT_EQUAL_STRING(e->key, "Amount");
    TEST_ASSERT_EQUAL_STRING(e->value, "1 ETH");
    TEST_ASSERT_EQUAL_PTR(e->extra_data, &sentinel);
    TEST_ASSERT_FALSE(e->start_intent);
    TEST_ASSERT_FALSE(e->is_separator);
    TEST_ASSERT_FALSE(e->end_intent);

    // Returned key/value pointers MUST be heap copies (not the caller's
    // string literals) — flipping the byte through the entry's pointer
    // would corrupt the read-only segment if they were the same.
    TEST_ASSERT(e->key != (void *) "Amount");
    TEST_ASSERT(e->value != (void *) "1 ETH");
}

void test_add_multiple_preserves_order(void) {
    TEST_ASSERT_TRUE(add_to_field_table(PARAM_TYPE_RAW, "k1", "v1", NULL));
    TEST_ASSERT_TRUE(add_to_field_table(PARAM_TYPE_RAW, "k2", "v2", NULL));
    TEST_ASSERT_TRUE(add_to_field_table(PARAM_TYPE_RAW, "k3", "v3", NULL));
    TEST_ASSERT_EQUAL(field_table_size(), 3);
    TEST_ASSERT_EQUAL_STRING(get_from_field_table(0)->key, "k1");
    TEST_ASSERT_EQUAL_STRING(get_from_field_table(1)->key, "k2");
    TEST_ASSERT_EQUAL_STRING(get_from_field_table(2)->key, "k3");
}

void test_add_end_intent_set_from_validator(void) {
    g_validate_hash_ret = true;  // validate_instruction_hash returns true
    TEST_ASSERT_TRUE(add_to_field_table(PARAM_TYPE_RAW, "k", "v", NULL));
    const s_field_table_entry *e = get_from_field_table(0);
    TEST_ASSERT_TRUE(e->end_intent);

    g_validate_hash_ret = false;
    TEST_ASSERT_TRUE(add_to_field_table(PARAM_TYPE_RAW, "k2", "v2", NULL));
    TEST_ASSERT_FALSE(get_from_field_table(1)->end_intent);
}

// =============================================================================
// add_to_field_table — special types
// =============================================================================

void test_add_intent_raises_flag_and_rewrites_type(void) {
    TEST_ASSERT_TRUE(add_to_field_table(PARAM_TYPE_INTENT, "TxKind", "Approve", NULL));
    const s_field_table_entry *e = get_from_field_table(0);
    TEST_ASSERT_TRUE(e->start_intent);
    // The stored type must be RAW even though the caller passed INTENT.
    TEST_ASSERT_EQUAL(e->type, PARAM_TYPE_RAW);
}

void test_add_separator_raises_flag_and_rewrites_type(void) {
    TEST_ASSERT_TRUE(add_to_field_table(PARAM_TYPE_SEPARATOR, "section", "", NULL));
    const s_field_table_entry *e = get_from_field_table(0);
    TEST_ASSERT_TRUE(e->is_separator);
    TEST_ASSERT_EQUAL(e->type, PARAM_TYPE_RAW);
}

// =============================================================================
// add_to_field_table — EIP-712 short-circuit
// =============================================================================

void test_add_in_eip712_routes_to_ui_helpers(void) {
    appState = APP_STATE_SIGNING_EIP712;
    TEST_ASSERT_TRUE(add_to_field_table(PARAM_TYPE_RAW, "From", "0xabc", NULL));
    // Table must NOT grow.
    TEST_ASSERT_EQUAL(field_table_size(), 0);
    // ui_712 helpers got the strings.
    TEST_ASSERT_EQUAL(g_ui_712_title_calls, 1);
    TEST_ASSERT_EQUAL(g_ui_712_value_calls, 1);
    TEST_ASSERT_EQUAL_STRING(g_ui_712_last_title, "From");
    TEST_ASSERT_EQUAL_STRING(g_ui_712_last_value, "0xabc");
    // ui_712_set_intent only fires for INTENT + batch > 1
    TEST_ASSERT_EQUAL(g_ui_712_intent_calls, 0);
}

void test_add_intent_in_eip712_with_batch_calls_set_intent(void) {
    appState = APP_STATE_SIGNING_EIP712;
    txContext.current_batch_size = 2;
    TEST_ASSERT_TRUE(add_to_field_table(PARAM_TYPE_INTENT, "TxKind", "Approve", NULL));
    TEST_ASSERT_EQUAL(g_ui_712_intent_calls, 1);
}

void test_add_intent_in_eip712_single_batch_skips_set_intent(void) {
    appState = APP_STATE_SIGNING_EIP712;
    txContext.current_batch_size = 1;
    TEST_ASSERT_TRUE(add_to_field_table(PARAM_TYPE_INTENT, "TxKind", "Approve", NULL));
    TEST_ASSERT_EQUAL(g_ui_712_intent_calls, 0);
}

// =============================================================================
// set_intent_field — picks INTENT vs RAW based on batch size
// =============================================================================

void test_set_intent_field_single_batch_uses_raw_type(void) {
    txContext.current_batch_size = 1;
    TEST_ASSERT_TRUE(set_intent_field("approve()"));
    const s_field_table_entry *e = get_from_field_table(0);
    TEST_ASSERT_EQUAL_STRING(e->key, "Transaction type");
    TEST_ASSERT_EQUAL_STRING(e->value, "approve()");
    // PARAM_TYPE_RAW means start_intent stays false (the RAW branch in
    // add_to_field_table does not raise it).
    TEST_ASSERT_FALSE(e->start_intent);
    TEST_ASSERT_EQUAL(e->type, PARAM_TYPE_RAW);
}

void test_set_intent_field_multi_batch_marks_intent(void) {
    txContext.current_batch_size = 3;
    TEST_ASSERT_TRUE(set_intent_field("approve()"));
    const s_field_table_entry *e = get_from_field_table(0);
    TEST_ASSERT_TRUE(e->start_intent);
    TEST_ASSERT_EQUAL(e->type, PARAM_TYPE_RAW);  // INTENT gets remapped to RAW
}

// =============================================================================
// Cleanup / get_from_field_table
// =============================================================================

void test_cleanup_empties_table(void) {
    add_to_field_table(PARAM_TYPE_RAW, "k", "v", NULL);
    add_to_field_table(PARAM_TYPE_RAW, "k2", "v2", NULL);
    TEST_ASSERT_EQUAL(field_table_size(), 2);
    field_table_cleanup();
    TEST_ASSERT_EQUAL(field_table_size(), 0);
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
    RUN_TEST(test_init_on_clean_succeeds);
    RUN_TEST(test_init_when_dirty_cleans_and_rejects);
    RUN_TEST(test_add_null_key_rejected);
    RUN_TEST(test_add_null_value_rejected);
    RUN_TEST(test_add_stores_copies_and_type);
    RUN_TEST(test_add_multiple_preserves_order);
    RUN_TEST(test_add_end_intent_set_from_validator);
    RUN_TEST(test_add_intent_raises_flag_and_rewrites_type);
    RUN_TEST(test_add_separator_raises_flag_and_rewrites_type);
    RUN_TEST(test_add_in_eip712_routes_to_ui_helpers);
    RUN_TEST(test_add_intent_in_eip712_with_batch_calls_set_intent);
    RUN_TEST(test_add_intent_in_eip712_single_batch_skips_set_intent);
    RUN_TEST(test_set_intent_field_single_batch_uses_raw_type);
    RUN_TEST(test_set_intent_field_multi_batch_marks_intent);
    RUN_TEST(test_cleanup_empties_table);
    return UNITY_END();
}
