/**
 * @file test_cmd_set_eth2_withdrawal_index.c
 * @brief Unit tests for handle_set_eth2_withdrawal_index at
 *        src/features/set_eth2_withdrawal_index/cmd_set_eth2_withdrawal_index.c.
 *
 * Trivial APDU: the host sets the Beacon-Chain withdrawal credential index
 * (uint32 BE). Pin the three checks: data length must be exactly 4,
 * P1/P2 must both be zero, and the success path stores the BE value into
 * `eth2WithdrawalIndex`.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "withdrawal_index.h"

static void reset(void) {
    eth2WithdrawalIndex = 0;
}

void test_wrong_data_length_rejected(void) {
    uint8_t data[3] = {0};
    TEST_ASSERT_EQUAL(handle_set_eth2_withdrawal_index(0, 0, data, 3), SWO_WRONG_DATA_LENGTH);
    TEST_ASSERT_EQUAL(handle_set_eth2_withdrawal_index(0, 0, data, 5), SWO_WRONG_DATA_LENGTH);
    // eth2WithdrawalIndex MUST stay untouched on a rejected APDU.
    TEST_ASSERT_EQUAL(eth2WithdrawalIndex, 0);
}

void test_wrong_p1_rejected(void) {
    uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    TEST_ASSERT_EQUAL(handle_set_eth2_withdrawal_index(1, 0, data, 4), SWO_WRONG_P1_P2);
    TEST_ASSERT_EQUAL(eth2WithdrawalIndex, 0);
}

void test_wrong_p2_rejected(void) {
    uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    TEST_ASSERT_EQUAL(handle_set_eth2_withdrawal_index(0, 1, data, 4), SWO_WRONG_P1_P2);
    TEST_ASSERT_EQUAL(eth2WithdrawalIndex, 0);
}

void test_success_stores_be_index(void) {
    uint8_t data[4] = {0x12, 0x34, 0x56, 0x78};
    TEST_ASSERT_EQUAL(handle_set_eth2_withdrawal_index(0, 0, data, 4), SWO_SUCCESS);
    TEST_ASSERT_EQUAL(eth2WithdrawalIndex, 0x12345678U);
}

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wrong_data_length_rejected);
    RUN_TEST(test_wrong_p1_rejected);
    RUN_TEST(test_wrong_p2_rejected);
    RUN_TEST(test_success_stores_be_index);
    return UNITY_END();
}
