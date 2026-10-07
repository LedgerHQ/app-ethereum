/**
 * @file test_cmd_get_app_configuration.c
 * @brief Unit tests for handle_get_app_configuration at
 *        src/features/get_app_configuration/cmd_get_app_configuration.c.
 *
 * The host calls GET_APP_CONFIGURATION at the start of every session
 * to learn:
 *   - which user-toggleable features are on (data signing allowed,
 *     transaction checks enabled / opted-in, external token needed),
 *   - which firmware version it is talking to.
 *
 * A bug here doesn't lose funds directly but it lies to the host
 * about the device's capability — leading to flows that get rejected
 * later in a confusing way (e.g. host streams typed data while
 * dataAllowed is actually off). Pin the wire layout.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "wraps.h"

static void reset(void) {
    memset(&g_n_storage_writable, 0, sizeof(g_n_storage_writable));
    memset(G_io_tx_buffer, 0, sizeof(G_io_tx_buffer));
}

void test_all_flags_off_only_external_token_set(void) {
    // dataAllowed = false, tx_check_enable = false, tx_check_opt_in = false.
    // APP_FLAG_EXTERNAL_TOKEN_NEEDED is unconditional.
    unsigned int tx = 0;
    uint16_t sw = handle_get_app_configuration(&tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(tx, 4);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], APP_FLAG_EXTERNAL_TOKEN_NEEDED);
}

void test_data_allowed_flag_propagated(void) {
    g_n_storage_writable.dataAllowed = true;
    unsigned int tx = 0;
    (void) handle_get_app_configuration(&tx);
    TEST_ASSERT_TRUE(G_io_tx_buffer[0] & APP_FLAG_DATA_ALLOWED);
    TEST_ASSERT_TRUE(G_io_tx_buffer[0] & APP_FLAG_EXTERNAL_TOKEN_NEEDED);
}

#ifdef HAVE_TRANSACTION_CHECKS
void test_tx_checks_enable_flag_propagated(void) {
    g_n_storage_writable.tx_check_enable = true;
    unsigned int tx = 0;
    (void) handle_get_app_configuration(&tx);
    TEST_ASSERT_TRUE(G_io_tx_buffer[0] & APP_FLAG_TX_CHECKS_ENABLE);
}

void test_tx_checks_opt_in_flag_propagated(void) {
    g_n_storage_writable.tx_check_opt_in = true;
    unsigned int tx = 0;
    (void) handle_get_app_configuration(&tx);
    TEST_ASSERT_TRUE(G_io_tx_buffer[0] & APP_FLAG_TX_CHECKS_OPT_IN);
}

void test_all_tx_check_flags_combine(void) {
    g_n_storage_writable.dataAllowed = true;
    g_n_storage_writable.tx_check_enable = true;
    g_n_storage_writable.tx_check_opt_in = true;
    unsigned int tx = 0;
    (void) handle_get_app_configuration(&tx);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0],
                      APP_FLAG_DATA_ALLOWED | APP_FLAG_TX_CHECKS_ENABLE |
                          APP_FLAG_TX_CHECKS_OPT_IN | APP_FLAG_EXTERNAL_TOKEN_NEEDED);
}
#endif

void test_response_length_is_exactly_four(void) {
    unsigned int tx = 42;  // sentinel
    (void) handle_get_app_configuration(&tx);
    // The handler overwrites `*tx` rather than adding to it.
    TEST_ASSERT_EQUAL(tx, 4);
}

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_all_flags_off_only_external_token_set);
    RUN_TEST(test_data_allowed_flag_propagated);
    RUN_TEST(test_tx_checks_enable_flag_propagated);
    RUN_TEST(test_tx_checks_opt_in_flag_propagated);
    RUN_TEST(test_all_tx_check_flags_combine);
    RUN_TEST(test_response_length_is_exactly_four);
    return UNITY_END();
}
