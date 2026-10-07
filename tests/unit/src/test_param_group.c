/**
 * @file test_param_group.c
 * @brief Unit tests for PARAM_GROUP formatting (iteration order, failure propagation)
 *
 * format_field is mocked so these tests exercise only GROUP-level logic:
 * sequential iteration, BUNDLED rejection, empty groups, and sub-field failure.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_field.h"
#include "gtp_param_group.h"

static bool g_format_field_ret = true;
static bool g_handle_field_struct_ret = true;
static bool g_verify_field_struct_ret = true;

// =============================================================================
// Mock functions
// =============================================================================

bool format_field(s_field *field, uint8_t depth) {
    return (bool) g_format_field_ret;
}

// Stubs for gtp_field.c symbols. The TLV-driven tests below use
// will_return() to flip each stub's outcome; the format-only tests
// at the top of the file never reach them.
static bool g_use_mock_handle_field = false;
static bool g_use_mock_verify_field = false;

bool handle_field_struct(const buffer_t *buf, s_field_ctx *ctx) {
    (void) buf;
    (void) ctx;
    if (g_use_mock_handle_field) {
        return (bool) g_handle_field_struct_ret;
    }
    return true;
}

bool verify_field_struct(const s_field_ctx *ctx) {
    (void) ctx;
    if (g_use_mock_verify_field) {
        return (bool) g_verify_field_struct_ret;
    }
    return true;
}

void cleanup_field(s_field *field) {
    (void) field;
}

void cleanup_field_constraints(s_field *field) {
    (void) field;
}

// =============================================================================
// Helpers
// =============================================================================

// Build a minimal s_field with name and param_type (other fields zeroed).
#define MAKE_FIELD(var, field_name, ptype) \
    s_field var;                           \
    memset(&(var), 0, sizeof(var));        \
    (var).version = 1;                     \
    (var).param_type = (ptype);            \
    strncpy((var).name, (field_name), sizeof((var).name) - 1);

// Build a s_group_field_node on the stack linking to next_node (or NULL).
#define MAKE_NODE(var, field_ptr, next_node_ptr)        \
    s_group_field_node var;                             \
    memset(&(var), 0, sizeof(var));                     \
    (var).node.next = (flist_node_t *) (next_node_ptr); \
    (var).field = (field_ptr);

// Build a top-level s_field wrapping a s_param_group.
static s_field make_group_field(e_group_iteration_type iter, s_group_field_node *head) {
    s_field outer;
    memset(&outer, 0, sizeof(outer));
    outer.version = 1;
    outer.param_type = PARAM_TYPE_GROUP;
    outer.param_group.version = 1;
    outer.param_group.iteration_type = iter;
    outer.param_group.fields = head;
    return outer;
}

// =============================================================================
// Test cases
// =============================================================================

void test_group_empty(void) {
    s_field outer = make_group_field(GROUP_ITER_SEQUENTIAL, NULL);
    // No sub-fields: format_field must never be called.
    TEST_ASSERT_TRUE(format_param_group(&outer, 0));
}

void test_group_sequential_one_subfield(void) {
    MAKE_FIELD(sub, "Amount", PARAM_TYPE_RAW);
    MAKE_NODE(node, &sub, NULL);
    s_field outer = make_group_field(GROUP_ITER_SEQUENTIAL, &node);
    g_format_field_ret = true;

    TEST_ASSERT_TRUE(format_param_group(&outer, 0));
}

void test_group_sequential_two_subfields(void) {
    MAKE_FIELD(sub2, "Value", PARAM_TYPE_RAW);
    MAKE_NODE(node2, &sub2, NULL);
    MAKE_FIELD(sub1, "Token ID", PARAM_TYPE_RAW);
    MAKE_NODE(node1, &sub1, &node2);
    s_field outer = make_group_field(GROUP_ITER_SEQUENTIAL, &node1);

    // Both sub-fields called in declaration order with the depth bumped by 1.
    g_format_field_ret = true;
    g_format_field_ret = true;

    TEST_ASSERT_TRUE(format_param_group(&outer, 0));
}

/**
 * BUNDLED iteration is unsupported and must be rejected explicitly
 * (no silent fallback to sequential rendering).
 */
void test_group_bundled_rejected(void) {
    MAKE_FIELD(sub2, "Value", PARAM_TYPE_RAW);
    MAKE_NODE(node2, &sub2, NULL);
    MAKE_FIELD(sub1, "Token ID", PARAM_TYPE_RAW);
    MAKE_NODE(node1, &sub1, &node2);
    s_field outer = make_group_field(GROUP_ITER_BUNDLED, &node1);

    // format_field must never be called when iteration type is BUNDLED.
    TEST_ASSERT_FALSE(format_param_group(&outer, 0));
}

void test_group_subfield_failure_propagates(void) {
    MAKE_FIELD(sub2, "Value", PARAM_TYPE_RAW);
    MAKE_NODE(node2, &sub2, NULL);
    MAKE_FIELD(sub1, "Token ID", PARAM_TYPE_RAW);
    MAKE_NODE(node1, &sub1, &node2);
    s_field outer = make_group_field(GROUP_ITER_SEQUENTIAL, &node1);

    // First sub-field fails; second must never be called.
    g_format_field_ret = false;

    TEST_ASSERT_FALSE(format_param_group(&outer, 0));
}

/**
 * Passing a depth at or above the cap must short-circuit before any sub-field
 * is visited. format_field() must never be invoked.
 */
void test_group_depth_cap_rejects(void) {
    MAKE_FIELD(sub, "Amount", PARAM_TYPE_RAW);
    MAKE_NODE(node, &sub, NULL);
    s_field outer = make_group_field(GROUP_ITER_SEQUENTIAL, &node);

    // 8 = MAX_PARAM_GROUP_DEPTH. No expect/will_return: format_field is
    // unreachable when the cap fires.
    TEST_ASSERT_FALSE(format_param_group(&outer, 8));
}

// =============================================================================
// TLV tag-handler tests — drive handle_param_group_struct with
// hand-crafted TLV buffers. handle_field_struct and verify_field_struct
// are wrapped so the FIELD sub-parser is observable.
// =============================================================================

void test_handle_pg_struct_version_and_iteration_ok(void) {
    uint8_t buf_bytes[] = {
        0x00,
        0x01,
        0x02,  // VERSION = 2
        0x01,
        0x01,
        GROUP_ITER_SEQUENTIAL,  // ITERATION_TYPE
    };
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_group param;
    memset(&param, 0, sizeof(param));
    s_param_group_context ctx = {.param = &param};
    TEST_ASSERT_TRUE(handle_param_group_struct(&buf, &ctx));
    TEST_ASSERT_EQUAL(param.version, 2);
    TEST_ASSERT_EQUAL(param.iteration_type, GROUP_ITER_SEQUENTIAL);
}

void test_handle_pg_struct_iteration_out_of_range_rejected(void) {
    uint8_t buf_bytes[] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x01,
        GROUP_ITER_MAX,  // outside [0, GROUP_ITER_MAX-1]
    };
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_group param;
    memset(&param, 0, sizeof(param));
    s_param_group_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_group_struct(&buf, &ctx));
}

void test_handle_pg_struct_field_appends_to_list(void) {
    g_use_mock_handle_field = true;
    g_use_mock_verify_field = true;
    g_handle_field_struct_ret = true;
    g_verify_field_struct_ret = true;

    uint8_t buf_bytes[] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x01,
        GROUP_ITER_SEQUENTIAL,
        0x02,
        0x00,  // FIELD (empty payload)
    };
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_group param;
    memset(&param, 0, sizeof(param));
    s_param_group_context ctx = {.param = &param};
    TEST_ASSERT_TRUE(handle_param_group_struct(&buf, &ctx));
    TEST_ASSERT_NOT_NULL(param.fields);
    cleanup_param_group(&param);
    g_use_mock_handle_field = false;
    g_use_mock_verify_field = false;
}

void test_handle_pg_struct_field_subparse_failure_rejected(void) {
    g_use_mock_handle_field = true;
    g_handle_field_struct_ret = false;

    uint8_t buf_bytes[] = {
        0x00,
        0x01,
        0x01,
        0x02,
        0x00,
    };
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_group param;
    memset(&param, 0, sizeof(param));
    s_param_group_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_group_struct(&buf, &ctx));
    TEST_ASSERT_NULL(param.fields);  // failure must NOT append.
    g_use_mock_handle_field = false;
}

void test_handle_pg_struct_field_verification_failure_rejected(void) {
    g_use_mock_handle_field = true;
    g_use_mock_verify_field = true;
    g_handle_field_struct_ret = true;
    g_verify_field_struct_ret = false;

    uint8_t buf_bytes[] = {
        0x00,
        0x01,
        0x01,
        0x02,
        0x00,
    };
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_group param;
    memset(&param, 0, sizeof(param));
    s_param_group_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_group_struct(&buf, &ctx));
    TEST_ASSERT_NULL(param.fields);
    g_use_mock_handle_field = false;
    g_use_mock_verify_field = false;
}

// =============================================================================
// Test runner
// =============================================================================

void setUp(void) {
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_group_empty);
    RUN_TEST(test_group_sequential_one_subfield);
    RUN_TEST(test_group_sequential_two_subfields);
    RUN_TEST(test_group_bundled_rejected);
    RUN_TEST(test_group_subfield_failure_propagates);
    RUN_TEST(test_group_depth_cap_rejects);
    RUN_TEST(test_handle_pg_struct_version_and_iteration_ok);
    RUN_TEST(test_handle_pg_struct_iteration_out_of_range_rejected);
    RUN_TEST(test_handle_pg_struct_field_appends_to_list);
    RUN_TEST(test_handle_pg_struct_field_subparse_failure_rejected);
    RUN_TEST(test_handle_pg_struct_field_verification_failure_rejected);
    return UNITY_END();
}
