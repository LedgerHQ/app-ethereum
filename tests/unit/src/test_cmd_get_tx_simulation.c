/**
 * @file test_cmd_get_tx_simulation.c
 * @brief Unit tests for the Ethereum-specific transaction-simulation logic
 *        at src/features/provide_tx_simulation/cmd_get_tx_simulation.c.
 *
 * The TLV parsing and signature verification are SDK responsibilities
 * (tlv_use_case_transaction_check).  This test wraps that SDK entry point
 * to inject controlled outputs and exercises only the app-level logic:
 *   - APDU dispatcher (handle_tx_simulation: P1 routing, opt-in)
 *   - Ethereum cross-checks (hash, address, chain_id, app-state)
 *   - Warning bit configuration (set_tx_simulation_warning)
 *   - String getters (get_tx_simulation_risk_str, get_tx_simulation_category_str)
 *   - Cleanup (clear_tx_simulation)
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "cmd_get_tx_simulation.h"
#include "apdu_constants.h"
#include "tlv_apdu.h"
#include "nbgl_use_case.h"
#include "wraps.h"
#include "Mocknetwork.h"

// =============================================================================
// Network mock state
// =============================================================================

static uint64_t s_tx_chain_id = 1;
static uint64_t get_tx_chain_id_stub(int cmock_num_calls) {
    (void) cmock_num_calls;
    return s_tx_chain_id;
}

// =============================================================================
// Controllable stubs
// =============================================================================

// --- SDK use-case wrap ---
// Wraps tlv_use_case_transaction_check to inject controlled results
// without exercising the SDK TLV parser / signature verifier.
static tlv_transaction_check_status_t g_uc_ret = TLV_TRANSACTION_CHECK_SUCCESS;
static transaction_check_risk_t g_uc_risk = TRANSACTION_CHECK_RISK_BENIGN;
static transaction_check_type_t g_uc_type = TRANSACTION_CHECK_TYPE_TRANSACTION;
static transaction_check_category_t g_uc_category = TRANSACTION_CHECK_CATEGORY_OTHERS;
static uint64_t g_uc_chain_id = 1;
static bool g_uc_chain_id_received = true;
static bool g_uc_domain_hash_received = false;
static bool g_uc_additional_data_received = false;
static const char *g_uc_partner = "Provider";

// Fixed buffers for the wrap to hand out via zero-copy pointers.
static uint8_t g_uc_tx_hash[CX_SHA3_256_SIZE];
static uint8_t g_uc_domain_hash[CX_SHA3_256_SIZE];
static uint8_t g_uc_address[ADDRESS_LENGTH];

tlv_transaction_check_status_t tlv_use_case_transaction_check(const buffer_t *payload,
                                                              tlv_transaction_check_out_t *out) {
    (void) payload;
    if (g_uc_ret != TLV_TRANSACTION_CHECK_SUCCESS) {
        return g_uc_ret;
    }
    out->risk = g_uc_risk;
    out->type = g_uc_type;
    out->category = g_uc_category;
    out->chain_id = g_uc_chain_id;
    out->chain_id_received = g_uc_chain_id_received;
    out->domain_hash_received = g_uc_domain_hash_received;
    out->additional_data_received = g_uc_additional_data_received;
    out->tx_hash.ptr = g_uc_tx_hash;
    out->tx_hash.size = sizeof(g_uc_tx_hash);
    out->domain_hash.ptr = g_uc_domain_hash;
    out->domain_hash.size = sizeof(g_uc_domain_hash);
    out->address.ptr = g_uc_address;
    out->address.size = sizeof(g_uc_address);
    strncpy(out->partner, g_uc_partner, sizeof(out->partner) - 1);
    out->partner[sizeof(out->partner) - 1] = '\0';
    return TLV_TRANSACTION_CHECK_SUCCESS;
}

// --- io_seproxyhal_send_status stub ---
static int g_send_status_calls = 0;
uint16_t io_seproxyhal_send_status(uint16_t sw, uint32_t tx, bool reset, bool idle) {
    (void) sw;
    (void) tx;
    (void) reset;
    (void) idle;
    g_send_status_calls++;
    return 0;
}

// --- UI opt-in stub ---
static int g_ui_opt_in_calls = 0;
static bool g_ui_opt_in_response_expected = false;
void ui_tx_simulation_opt_in(bool response_expected) {
    g_ui_opt_in_calls++;
    g_ui_opt_in_response_expected = response_expected;
}

// --- get_public_key wrap ---
static uint8_t g_pubkey_addr[ADDRESS_LENGTH];
uint16_t get_public_key(uint8_t *out, uint8_t out_size) {
    if (out_size < ADDRESS_LENGTH) {
        return SWO_INCORRECT_DATA;
    }
    memcpy(out, g_pubkey_addr, ADDRESS_LENGTH);
    return SWO_SUCCESS;
}

// =============================================================================
// Helpers
// =============================================================================

// Send a minimal first-chunk APDU that goes through tlv_from_apdu.
// The actual TLV content doesn't matter because the SDK parser is wrapped.
static bool send_descriptor(void) {
    // tlv_from_apdu expects a 2-byte BE length prefix followed by payload.
    // We send a trivial 1-byte "payload" — the wrap ignores it.
    uint8_t framed[3] = {0x00, 0x01, 0x00};
    uint16_t sw = handle_tx_simulation(/*p1=*/0x00, /*p2=*/P1_FIRST_CHUNK, framed, sizeof(framed));
    return sw == SWO_SUCCESS;
}

// Prime a successful descriptor and matching signing context for
// set_tx_simulation_warning cross-check tests.
static void prime_for_warning(transaction_check_risk_t risk) {
    g_uc_risk = risk;
    g_uc_type = TRANSACTION_CHECK_TYPE_TRANSACTION;
    g_uc_category = TRANSACTION_CHECK_CATEGORY_OTHERS;
    g_uc_chain_id = 1;
    g_uc_chain_id_received = true;
    g_uc_domain_hash_received = false;
    g_uc_additional_data_received = false;
    memset(g_uc_tx_hash, 0xBB, sizeof(g_uc_tx_hash));
    memset(g_uc_address, 0xAA, sizeof(g_uc_address));
    TEST_ASSERT_TRUE(send_descriptor());
    // Mirror the same values into the active signing context.
    memset(tmpCtx.transactionContext.hash, 0xBB, INT256_LENGTH);
    appState = APP_STATE_SIGNING_TX;
    s_tx_chain_id = 1;
    memset(g_pubkey_addr, 0xAA, ADDRESS_LENGTH);
}

static void prime_for_warning_typed_data(transaction_check_risk_t risk) {
    g_uc_risk = risk;
    g_uc_type = TRANSACTION_CHECK_TYPE_TYPED_DATA;
    g_uc_category = TRANSACTION_CHECK_CATEGORY_OTHERS;
    g_uc_chain_id_received = false;
    g_uc_domain_hash_received = true;
    g_uc_additional_data_received = false;
    memset(g_uc_tx_hash, 0xBB, sizeof(g_uc_tx_hash));
    memset(g_uc_domain_hash, 0xCC, sizeof(g_uc_domain_hash));
    memset(g_uc_address, 0xAA, sizeof(g_uc_address));
    TEST_ASSERT_TRUE(send_descriptor());
    memset(tmpCtx.messageSigningContext712.messageHash, 0xBB, INT256_LENGTH);
    memset(tmpCtx.messageSigningContext712.domainHash, 0xCC, INT256_LENGTH);
    appState = APP_STATE_SIGNING_EIP712;
    memset(g_pubkey_addr, 0xAA, ADDRESS_LENGTH);
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    clear_tx_simulation();
    memset(&g_n_storage_writable, 0, sizeof(g_n_storage_writable));
    g_n_storage_writable.tx_check_enable = true;
    g_n_storage_writable.tx_check_opt_in = true;
    g_uc_ret = TLV_TRANSACTION_CHECK_SUCCESS;
    g_uc_risk = TRANSACTION_CHECK_RISK_BENIGN;
    g_uc_type = TRANSACTION_CHECK_TYPE_TRANSACTION;
    g_uc_category = TRANSACTION_CHECK_CATEGORY_OTHERS;
    g_uc_chain_id = 1;
    g_uc_chain_id_received = true;
    g_uc_domain_hash_received = false;
    g_uc_additional_data_received = false;
    g_uc_partner = "Provider";
    memset(g_uc_tx_hash, 0xBB, sizeof(g_uc_tx_hash));
    memset(g_uc_domain_hash, 0, sizeof(g_uc_domain_hash));
    memset(g_uc_address, 0xAA, sizeof(g_uc_address));
    g_send_status_calls = 0;
    g_ui_opt_in_calls = 0;
    g_ui_opt_in_response_expected = false;
    memset(g_pubkey_addr, 0xAA, ADDRESS_LENGTH);
    s_tx_chain_id = 1;
    appState = APP_STATE_SIGNING_TX;
    // tlv_apdu carries internal state across calls — clear it.
    tlv_from_apdu(false, 0, NULL, NULL);
}

// =============================================================================
// Tests — APDU dispatcher
// =============================================================================

void test_p1_unknown_rejected(void) {
    uint8_t data[1] = {0};
    uint16_t sw = handle_tx_simulation(/*p1=*/0xFF, /*p2=*/0, data, 1);
    TEST_ASSERT_EQUAL(sw, SWO_WRONG_P1_P2);
}

void test_p1_data_when_checks_disabled_returns_not_supported(void) {
    g_n_storage_writable.tx_check_enable = false;
    uint8_t data[1] = {0};
    uint16_t sw = handle_tx_simulation(/*p1=*/0x00, /*p2=*/P1_FIRST_CHUNK, data, 1);
    TEST_ASSERT_EQUAL(sw, SWO_COMMAND_CODE_NOT_SUPPORTED);
}

void test_p1_opt_in_already_optin_short_circuits(void) {
    g_n_storage_writable.tx_check_opt_in = true;
    g_n_storage_writable.tx_check_enable = true;
    appState = APP_STATE_IDLE;
    uint8_t data[1] = {0};
    uint16_t sw = handle_tx_simulation(/*p1=*/0x01, /*p2=*/0, data, 1);
    TEST_ASSERT_EQUAL(sw, SWO_NO_RESPONSE);
    TEST_ASSERT_EQUAL(g_send_status_calls, 1);
    TEST_ASSERT_EQUAL(g_ui_opt_in_calls, 0);
}

void test_p1_opt_in_not_yet_optin_calls_ui(void) {
    g_n_storage_writable.tx_check_opt_in = false;
    appState = APP_STATE_IDLE;
    uint8_t data[1] = {0};
    uint16_t sw = handle_tx_simulation(/*p1=*/0x01, /*p2=*/0, data, 1);
    TEST_ASSERT_EQUAL(sw, SWO_NO_RESPONSE);
    TEST_ASSERT_EQUAL(g_ui_opt_in_calls, 1);
    TEST_ASSERT_TRUE(g_ui_opt_in_response_expected);
}

void test_p1_data_rejected_when_already_received(void) {
    TEST_ASSERT_TRUE(send_descriptor());
    // A second provisioning while received==true must be rejected so the host
    // cannot overwrite the displayed warning after the review is on screen.
    TEST_ASSERT_FALSE(send_descriptor());
}

void test_p1_opt_in_rejected_when_app_not_idle(void) {
    appState = APP_STATE_SIGNING_TX;
    uint8_t data[1] = {0};
    uint16_t sw = handle_tx_simulation(/*p1=*/0x01, /*p2=*/0, data, 1);
    TEST_ASSERT_EQUAL(sw, SWO_COMMAND_NOT_ALLOWED);
    TEST_ASSERT_EQUAL(g_ui_opt_in_calls, 0);
}

// =============================================================================
// Tests — app-level rejection of SDK output
// =============================================================================

void test_sdk_failure_rejects(void) {
    g_uc_ret = TLV_TRANSACTION_CHECK_PARSING_ERROR;
    TEST_ASSERT_FALSE(send_descriptor());
}

void test_additional_data_rejected(void) {
    g_uc_additional_data_received = true;
    TEST_ASSERT_FALSE(send_descriptor());
}

void test_transaction_without_chain_id_rejected(void) {
    g_uc_type = TRANSACTION_CHECK_TYPE_TRANSACTION;
    g_uc_chain_id_received = false;
    TEST_ASSERT_FALSE(send_descriptor());
}

void test_typed_data_without_domain_hash_rejected(void) {
    g_uc_type = TRANSACTION_CHECK_TYPE_TYPED_DATA;
    g_uc_domain_hash_received = false;
    TEST_ASSERT_FALSE(send_descriptor());
}

// =============================================================================
// Tests — string getters
// =============================================================================

void test_get_risk_str_for_each_value(void) {
    g_uc_risk = TRANSACTION_CHECK_RISK_BENIGN;
    TEST_ASSERT_TRUE(send_descriptor());
    TEST_ASSERT_EQUAL_STRING(get_tx_simulation_risk_str(), "BENIGN");

    clear_tx_simulation();
    g_uc_risk = TRANSACTION_CHECK_RISK_WARNING;
    TEST_ASSERT_TRUE(send_descriptor());
    TEST_ASSERT_EQUAL_STRING(get_tx_simulation_risk_str(), "RISK (WARNING)");

    clear_tx_simulation();
    g_uc_risk = TRANSACTION_CHECK_RISK_MALICIOUS;
    TEST_ASSERT_TRUE(send_descriptor());
    TEST_ASSERT_EQUAL_STRING(get_tx_simulation_risk_str(), "THREAT (MALICIOUS)");

    clear_tx_simulation();
    TEST_ASSERT_EQUAL_STRING(get_tx_simulation_risk_str(), "BENIGN");
}

void test_get_category_str_warning_branches(void) {
    g_uc_risk = TRANSACTION_CHECK_RISK_WARNING;

    g_uc_category = TRANSACTION_CHECK_CATEGORY_ADDRESS;
    TEST_ASSERT_TRUE(send_descriptor());
    TEST_ASSERT_EQUAL_STRING(get_tx_simulation_category_str(),
                             "This transaction involves a suspicious address. "
                             "It might not be safe to continue.");

    clear_tx_simulation();
    g_uc_category = TRANSACTION_CHECK_CATEGORY_DAPP;
    TEST_ASSERT_TRUE(send_descriptor());
    TEST_ASSERT_EQUAL_STRING(get_tx_simulation_category_str(),
                             "This transaction involves a suspicious dApp. "
                             "It might not be safe to continue.");

    clear_tx_simulation();
    g_uc_category = TRANSACTION_CHECK_CATEGORY_LOSING_OPERATION;
    TEST_ASSERT_TRUE(send_descriptor());
    TEST_ASSERT_EQUAL_STRING(get_tx_simulation_category_str(),
                             "This transaction could end in a loss. "
                             "Check transaction details carefully before signing.");
}

void test_get_category_str_malicious_branches(void) {
    g_uc_risk = TRANSACTION_CHECK_RISK_MALICIOUS;

    g_uc_category = TRANSACTION_CHECK_CATEGORY_ADDRESS;
    TEST_ASSERT_TRUE(send_descriptor());
    TEST_ASSERT_EQUAL_STRING(get_tx_simulation_category_str(),
                             "This transaction involves a malicious address. "
                             "Your assets will most likely be stolen.");

    clear_tx_simulation();
    g_uc_category = TRANSACTION_CHECK_CATEGORY_DAPP;
    TEST_ASSERT_TRUE(send_descriptor());
    TEST_ASSERT_EQUAL_STRING(get_tx_simulation_category_str(),
                             "This dApp is linked to a scammer. "
                             "Your assets will most likely be stolen.");
}

void test_clear_tx_simulation_zeroes_struct(void) {
    g_uc_chain_id = 42;
    g_uc_risk = TRANSACTION_CHECK_RISK_WARNING;
    TEST_ASSERT_TRUE(send_descriptor());
    clear_tx_simulation();
    TEST_ASSERT_EQUAL_STRING(get_tx_simulation_risk_str(), "BENIGN");
}

// =============================================================================
// Tests — set_tx_simulation_warning cross-checks
// =============================================================================

void test_set_warning_disabled_returns_early(void) {
    prime_for_warning(TRANSACTION_CHECK_RISK_MALICIOUS);
    warning.predefinedSet = 0;
    g_n_storage_writable.tx_check_enable = false;
    set_tx_simulation_warning();
    TEST_ASSERT_EQUAL(warning.predefinedSet, 0);
    TEST_ASSERT_NULL(warning.reportProvider);
}

void test_set_warning_risk_benign_sets_no_threat_bit(void) {
    prime_for_warning(TRANSACTION_CHECK_RISK_BENIGN);
    warning.predefinedSet = 0;
    set_tx_simulation_warning();
    TEST_ASSERT_EQUAL(warning.predefinedSet, 1U << W3C_NO_THREAT_WARN);
}

void test_set_warning_risk_warning_sets_risk_bit(void) {
    prime_for_warning(TRANSACTION_CHECK_RISK_WARNING);
    warning.predefinedSet = 0;
    set_tx_simulation_warning();
    TEST_ASSERT_EQUAL(warning.predefinedSet, 1U << W3C_RISK_DETECTED_WARN);
}

void test_set_warning_risk_malicious_sets_threat_bit(void) {
    prime_for_warning(TRANSACTION_CHECK_RISK_MALICIOUS);
    warning.predefinedSet = 0;
    set_tx_simulation_warning();
    TEST_ASSERT_EQUAL(warning.predefinedSet, 1U << W3C_THREAT_DETECTED_WARN);
}

void test_set_warning_address_mismatch_forces_issue(void) {
    prime_for_warning(TRANSACTION_CHECK_RISK_BENIGN);
    memset(g_pubkey_addr, 0x77, ADDRESS_LENGTH);
    warning.predefinedSet = 0;
    set_tx_simulation_warning();
    TEST_ASSERT_EQUAL(warning.predefinedSet, 1U << W3C_ISSUE_WARN);
}

void test_set_warning_tx_hash_mismatch_forces_issue(void) {
    prime_for_warning(TRANSACTION_CHECK_RISK_BENIGN);
    memset(tmpCtx.transactionContext.hash, 0x99, INT256_LENGTH);
    warning.predefinedSet = 0;
    set_tx_simulation_warning();
    TEST_ASSERT_EQUAL(warning.predefinedSet, 1U << W3C_ISSUE_WARN);
}

void test_set_warning_chain_id_mismatch_forces_issue(void) {
    prime_for_warning(TRANSACTION_CHECK_RISK_BENIGN);
    s_tx_chain_id = 137;
    warning.predefinedSet = 0;
    set_tx_simulation_warning();
    TEST_ASSERT_EQUAL(warning.predefinedSet, 1U << W3C_ISSUE_WARN);
}

void test_set_warning_wrong_app_state_forces_issue(void) {
    prime_for_warning(TRANSACTION_CHECK_RISK_BENIGN);
    appState = APP_STATE_SIGNING_EIP712;
    warning.predefinedSet = 0;
    set_tx_simulation_warning();
    TEST_ASSERT_EQUAL(warning.predefinedSet, 1U << W3C_ISSUE_WARN);
}

void test_set_warning_typed_data_match_sets_warning_bit(void) {
    prime_for_warning_typed_data(TRANSACTION_CHECK_RISK_WARNING);
    warning.predefinedSet = 0;
    set_tx_simulation_warning();
    TEST_ASSERT_EQUAL(warning.predefinedSet, 1U << W3C_RISK_DETECTED_WARN);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    Mocknetwork_Init();
    get_tx_chain_id_StubWithCallback(get_tx_chain_id_stub);
    reset();
}
void tearDown(void) {
    Mocknetwork_Verify();
    Mocknetwork_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_p1_unknown_rejected);
    RUN_TEST(test_p1_data_when_checks_disabled_returns_not_supported);
    RUN_TEST(test_p1_opt_in_already_optin_short_circuits);
    RUN_TEST(test_p1_opt_in_not_yet_optin_calls_ui);
    RUN_TEST(test_p1_data_rejected_when_already_received);
    RUN_TEST(test_p1_opt_in_rejected_when_app_not_idle);
    RUN_TEST(test_sdk_failure_rejects);
    RUN_TEST(test_additional_data_rejected);
    RUN_TEST(test_transaction_without_chain_id_rejected);
    RUN_TEST(test_typed_data_without_domain_hash_rejected);
    RUN_TEST(test_get_risk_str_for_each_value);
    RUN_TEST(test_get_category_str_warning_branches);
    RUN_TEST(test_get_category_str_malicious_branches);
    RUN_TEST(test_clear_tx_simulation_zeroes_struct);
    RUN_TEST(test_set_warning_disabled_returns_early);
    RUN_TEST(test_set_warning_risk_benign_sets_no_threat_bit);
    RUN_TEST(test_set_warning_risk_warning_sets_risk_bit);
    RUN_TEST(test_set_warning_risk_malicious_sets_threat_bit);
    RUN_TEST(test_set_warning_address_mismatch_forces_issue);
    RUN_TEST(test_set_warning_tx_hash_mismatch_forces_issue);
    RUN_TEST(test_set_warning_chain_id_mismatch_forces_issue);
    RUN_TEST(test_set_warning_wrong_app_state_forces_issue);
    RUN_TEST(test_set_warning_typed_data_match_sets_warning_bit);
    return UNITY_END();
}
