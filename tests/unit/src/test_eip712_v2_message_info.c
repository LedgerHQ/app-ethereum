/**
 * @file test_eip712_v2_message_info.c
 * @brief Unit tests for the EIP712_MESSAGE_INFO command of EIP-712 V2:
 *        src/features/sign_message_eip712/cmd_eip712_v2_message_info.c (the APDU and its state
 *        rules) and eip712_v2_message_info.c (the binding of the struct to the message).
 *
 * The struct itself is parsed and verified by operation_info.c, which test_tx_info covers, and the
 * typed data it is bound to lives in typed_data.c: both are stubbed here. So is the context that
 * keeps the accepted message, which test_operation_ctx covers. tlv_from_apdu is the seam through
 * which the payload handler is reached.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "cmd_eip712_v2_message_info.h"
#include "eip712_v2_message_info.h"
#include "eip712_session.h"
#include "operation_info.h"
#include "typed_data.h"
#include "type_hash.h"
#include "operation_ctx.h"
#include "apdu_constants.h"
#include "shared_context.h"
#include "chain_config.h"
#include "status_words.h"
#include "Mocktlv_apdu.h"

// =============================================================================
// Stubs
// =============================================================================

// the typed data the info is bound to
static bool g_has_schema = true;
static bool g_has_domain = true;
static bool g_has_message = true;
static const char *g_message_type_name = "Mail";
static bool g_domain_has_chain_id = true;
static bool g_domain_chain_id_is_a_number = true;
static uint64_t g_domain_chain_id = 1;
static bool g_domain_has_contract = true;
static bool g_domain_contract_is_an_address = true;
static uint8_t g_domain_contract[ADDRESS_LENGTH];

bool td_has_schema(void) {
    return g_has_schema;
}
bool td_has_domain(void) {
    return g_has_domain;
}
bool td_has_message(void) {
    return g_has_message;
}
const char *td_get_message_type_name(void) {
    return g_message_type_name;
}
bool td_get_domain_chain_id(uint64_t *chain_id) {
    if (!g_domain_has_chain_id || !g_domain_chain_id_is_a_number) {
        return false;
    }
    *chain_id = g_domain_chain_id;
    return true;
}
bool td_get_domain_contract_addr(uint8_t addr[ADDRESS_LENGTH]) {
    if (!g_domain_has_contract) {
        return false;
    }
    memcpy(addr, g_domain_contract, ADDRESS_LENGTH);
    return true;
}
// the contract of the domain when it is an address, which the Ethermint placeholder is not
bool td_get_domain_verifying_address(uint8_t addr[ADDRESS_LENGTH]) {
    if (!g_domain_has_contract || !g_domain_contract_is_an_address) {
        return false;
    }
    memcpy(addr, g_domain_contract, ADDRESS_LENGTH);
    return true;
}

// the hash of the domain
static bool g_domain_separator_ret = true;
static uint8_t g_domain_separator[CX_KECCAK_256_SIZE];
bool td_hash_domain(uint8_t out[CX_KECCAK_256_SIZE]) {
    memcpy(out, g_domain_separator, CX_KECCAK_256_SIZE);
    return g_domain_separator_ret;
}

// the type hash of the message's type
static bool g_type_hash_ret = true;
static uint8_t g_type_hash[INT256_LENGTH];
bool type_hash(const char *struct_name, uint8_t *hash_buf) {
    (void) struct_name;
    memcpy(hash_buf, g_type_hash, sizeof(g_type_hash));
    return g_type_hash_ret;
}

// the struct, as operation_info.c would hand it over
static bool g_receive_ret = true;
static s_operation_info g_received;
static int g_delete_calls = 0;
s_operation_info *new_operation_info(const buffer_t *buf, e_operation_info_context context) {
    s_operation_info *info;

    (void) buf;
    if (!g_receive_ret) {
        return NULL;
    }
    info = malloc(sizeof(*info));
    memcpy(info, &g_received, sizeof(*info));
    info->context = context;
    return info;
}
void delete_operation_info(s_operation_info *node) {
    g_delete_calls++;
    free(node);
}
const uint8_t *get_domain_separator(const s_operation_info *tx_info) {
    return tx_info->domain_separator;
}
const uint8_t *get_primary_type_hash(const s_operation_info *tx_info) {
    return tx_info->primary_type_hash;
}

// the context the accepted message is made the root of, which takes the info over either way
static bool g_init_message_ret = true;
static s_operation_info *g_message = NULL;
static uint8_t g_message_from[ADDRESS_LENGTH];
static bool g_message_has_contract = false;
static uint8_t g_message_contract[ADDRESS_LENGTH];
static bool g_message_has_chain = false;
static uint64_t g_message_chain = 0;
bool operation_ctx_init_message(s_operation_info *info,
                                const uint8_t from[ADDRESS_LENGTH],
                                const uint8_t contract[ADDRESS_LENGTH],
                                const uint64_t *chain_id) {
    if (!g_init_message_ret) {
        delete_operation_info(info);
        return false;
    }
    g_message = info;
    memcpy(g_message_from, from, ADDRESS_LENGTH);
    g_message_has_contract = (contract != NULL);
    if (contract != NULL) {
        memcpy(g_message_contract, contract, ADDRESS_LENGTH);
    }
    g_message_has_chain = (chain_id != NULL);
    if (chain_id != NULL) {
        g_message_chain = *chain_id;
    }
    return true;
}
bool operation_ctx_has_message(void) {
    return g_message != NULL;
}

// the account of a path, which the message is signed with
static const uint8_t g_signer[ADDRESS_LENGTH] = {0x51, 0x67, 0x4E, 0x52};
static bool g_derive_ret = true;
static const bip32_path_t *g_derived_path = NULL;
bool get_address_of_path(const bip32_path_t *bip32, uint8_t out[ADDRESS_LENGTH]) {
    g_derived_path = bip32;
    if (!g_derive_ret) {
        return false;
    }
    memcpy(out, g_signer, ADDRESS_LENGTH);
    return true;
}

// a new signing flow starts with no message
static void forget_the_message(void) {
    if (g_message != NULL) {
        delete_operation_info(g_message);
        g_message = NULL;
    }
}

// tlv_from_apdu: the stub runs the handler the command gave, as if the payload were complete
static e_tlv_apdu_ret g_tlv_ret = TLV_APDU_SUCCESS;
static bool g_tlv_run_handler = true;
static int g_tlv_calls = 0;
static e_tlv_apdu_ret tlv_from_apdu_stub(bool first_chunk,
                                         uint8_t lc,
                                         const uint8_t *payload,
                                         f_tlv_payload_handler handler,
                                         int cmock_num_calls) {
    buffer_t buf = {0};

    (void) first_chunk;
    (void) lc;
    (void) payload;
    (void) cmock_num_calls;
    g_tlv_calls++;
    if (g_tlv_run_handler && !handler(&buf)) {
        return TLV_APDU_ERROR;
    }
    return g_tlv_ret;
}

// =============================================================================
// Fixtures
// =============================================================================

static const uint8_t g_type_hash_of_mail[INT256_LENGTH] = {0x4D, 0x41, 0x49, 0x4C, 0x01};
static const uint8_t g_hash_of_the_domain[CX_KECCAK_256_SIZE] = {0xD0, 0x4A, 0x14, 0x01};

static void reset(void) {
    appState = APP_STATE_IDLE;
    eip712_session_close();
    TEST_ASSERT_TRUE(eip712_session_open(EIP712_SESSION_V2));
    g_has_schema = true;
    g_has_domain = true;
    g_has_message = true;
    g_message_type_name = "Mail";
    g_domain_has_chain_id = true;
    g_domain_chain_id_is_a_number = true;
    g_domain_chain_id = 1;
    g_domain_has_contract = true;
    g_domain_contract_is_an_address = true;
    memset(g_domain_contract, 0xAB, sizeof(g_domain_contract));
    g_domain_separator_ret = true;
    memcpy(g_domain_separator, g_hash_of_the_domain, sizeof(g_domain_separator));
    g_type_hash_ret = true;
    memcpy(g_type_hash, g_type_hash_of_mail, sizeof(g_type_hash));
    g_receive_ret = true;
    // a message info for that domain and type
    memset(&g_received, 0, sizeof(g_received));
    memcpy(g_received.domain_separator, g_hash_of_the_domain, sizeof(g_hash_of_the_domain));
    memcpy(g_received.primary_type_hash, g_type_hash_of_mail, sizeof(g_type_hash_of_mail));
    g_init_message_ret = true;
    g_derive_ret = true;
    g_derived_path = NULL;
    memset(g_message_from, 0, sizeof(g_message_from));
    g_message_has_contract = false;
    memset(g_message_contract, 0, sizeof(g_message_contract));
    g_message_has_chain = false;
    g_message_chain = 0;
    g_delete_calls = 0;
    g_tlv_ret = TLV_APDU_SUCCESS;
    g_tlv_run_handler = true;
    g_tlv_calls = 0;
    forget_the_message();
}

static uint16_t send(uint8_t p1) {
    return handle_eip712_v2_message_info(p1, P2_EIP712_V2_IMPLEM, 0, NULL);
}

// =============================================================================
// The command: parameters and state
// =============================================================================

void test_wrong_p2_rejected(void) {
    TEST_ASSERT_EQUAL(handle_eip712_v2_message_info(P1_FIRST_CHUNK, 0, 0, NULL),
                      SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(g_tlv_calls, 0);
}

void test_unknown_p1_rejected(void) {
    TEST_ASSERT_EQUAL(send(0x02), SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(g_tlv_calls, 0);
}

void test_v1_session_refused(void) {
    appState = APP_STATE_IDLE;
    eip712_session_close();
    TEST_ASSERT_TRUE(eip712_session_open(EIP712_SESSION_V1));
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_COMMAND_NOT_ALLOWED);
    TEST_ASSERT_EQUAL(g_tlv_calls, 0);
}

void test_no_session_refused(void) {
    eip712_session_close();
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_COMMAND_NOT_ALLOWED);
    TEST_ASSERT_EQUAL(g_tlv_calls, 0);
}

void test_wrong_app_state_refused(void) {
    const uint8_t states[] = {APP_STATE_IDLE, APP_STATE_SIGNING_EIP712, APP_STATE_SIGNING_TX};

    for (size_t i = 0; i < sizeof(states); i++) {
        appState = states[i];
        TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_COMMAND_NOT_ALLOWED);
    }
    TEST_ASSERT_EQUAL(g_tlv_calls, 0);
}

void test_missing_schema_domain_or_message_refused(void) {
    g_has_schema = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_COMMAND_NOT_ALLOWED);
    g_has_schema = true;
    g_has_domain = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_COMMAND_NOT_ALLOWED);
    g_has_domain = true;
    g_has_message = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_COMMAND_NOT_ALLOWED);
    TEST_ASSERT_EQUAL(g_tlv_calls, 0);
}

void test_tlv_failure_is_incorrect_data(void) {
    g_tlv_run_handler = false;
    g_tlv_ret = TLV_APDU_ERROR;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_INCORRECT_DATA);
    TEST_ASSERT_NULL(g_message);
}

void test_pending_chunk_succeeds_without_storing(void) {
    g_tlv_run_handler = false;
    g_tlv_ret = TLV_APDU_PENDING;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_SUCCESS);
    TEST_ASSERT_NULL(g_message);
}

void test_second_message_info_refused(void) {
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_SUCCESS);
    TEST_ASSERT_NOT_NULL(g_message);
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_COMMAND_NOT_ALLOWED);
    TEST_ASSERT_EQUAL(g_tlv_calls, 1);
}

// =============================================================================
// The binding
// =============================================================================

void test_info_for_the_message_accepted_and_kept(void) {
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_SUCCESS);
    TEST_ASSERT_NOT_NULL(g_message);
    TEST_ASSERT_EQUAL(g_message->context, OPERATION_INFO_CTX_EIP712);
    TEST_ASSERT_EQUAL(g_delete_calls, 0);
}

void test_receive_failure_stores_nothing(void) {
    g_receive_ret = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_INCORRECT_DATA);
    TEST_ASSERT_NULL(g_message);
}

void test_other_type_rejected_and_released(void) {
    g_received.primary_type_hash[0] ^= 0x01;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_INCORRECT_DATA);
    TEST_ASSERT_NULL(g_message);
    TEST_ASSERT_EQUAL(g_delete_calls, 1);
}

void test_type_hash_failure_or_no_message_rejected(void) {
    g_type_hash_ret = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_INCORRECT_DATA);
    g_type_hash_ret = true;
    g_message_type_name = NULL;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_INCORRECT_DATA);
    TEST_ASSERT_NULL(g_message);
}

void test_other_domain_rejected_and_released(void) {
    g_received.domain_separator[CX_KECCAK_256_SIZE - 1] ^= 0x01;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_INCORRECT_DATA);
    TEST_ASSERT_NULL(g_message);
    TEST_ASSERT_EQUAL(g_delete_calls, 1);
}

void test_domain_separator_failure_rejected(void) {
    g_domain_separator_ret = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_INCORRECT_DATA);
    TEST_ASSERT_NULL(g_message);
}

// The domain separator covers the chain and the contract, which a domain may not have
void test_domain_with_neither_chain_nor_contract_accepted(void) {
    g_domain_has_chain_id = false;
    g_domain_has_contract = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_SUCCESS);
    TEST_ASSERT_NOT_NULL(g_message);
}

// =============================================================================
// The message made of the info
// =============================================================================

// the calls of the message go to the contract of its domain
void test_the_contract_of_the_domain_is_handed_over(void) {
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_SUCCESS);
    TEST_ASSERT_TRUE(g_message_has_contract);
    TEST_ASSERT_EQUAL_MEMORY(g_domain_contract, g_message_contract, ADDRESS_LENGTH);
}

void test_no_contract_is_handed_over_when_the_domain_has_none(void) {
    g_domain_has_contract = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_SUCCESS);
    TEST_ASSERT_FALSE(g_message_has_contract);
}

// the Ethermint placeholder is the contract of the domain, but not an address to call
void test_no_contract_is_handed_over_when_the_domain_one_is_not_an_address(void) {
    g_domain_contract_is_an_address = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_SUCCESS);
    TEST_ASSERT_NOT_NULL(g_message);
    TEST_ASSERT_FALSE(g_message_has_contract);
}

// the message is on the chain of its domain
void test_the_chain_of_the_domain_is_handed_over(void) {
    g_domain_chain_id = 137;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_SUCCESS);
    TEST_ASSERT_TRUE(g_message_has_chain);
    TEST_ASSERT_EQUAL_UINT64(137, g_message_chain);
}

void test_no_chain_is_handed_over_when_the_domain_has_none(void) {
    g_domain_has_chain_id = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_SUCCESS);
    TEST_ASSERT_FALSE(g_message_has_chain);
}

// a chainId that is not a number, 0 or above the maximum is no chain
void test_no_chain_is_handed_over_when_the_domain_one_is_not_a_chain(void) {
    const uint64_t chains[] = {0, MAX_VALID_CHAIN_ID + 1};

    g_domain_chain_id_is_a_number = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_SUCCESS);
    TEST_ASSERT_FALSE(g_message_has_chain);
    g_domain_chain_id_is_a_number = true;
    for (size_t i = 0; i < ARRAYLEN(chains); i++) {
        forget_the_message();
        g_domain_chain_id = chains[i];
        TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_SUCCESS);
        TEST_ASSERT_FALSE(g_message_has_chain);
    }
}

// the message is signed with the path that came with its values
void test_the_signer_is_the_account_of_the_signing_path(void) {
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_SUCCESS);
    TEST_ASSERT_EQUAL_PTR(&tmpCtx.messageSigningContext712.bip32, g_derived_path);
    TEST_ASSERT_EQUAL_MEMORY(g_signer, g_message_from, ADDRESS_LENGTH);
}

// a message that cannot be made is not accepted, and the info is not left behind
void test_message_that_cannot_be_made_is_refused(void) {
    g_init_message_ret = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_INCORRECT_DATA);
    TEST_ASSERT_NULL(g_message);
    TEST_ASSERT_EQUAL(g_delete_calls, 1);
}

void test_message_without_a_signer_is_refused(void) {
    g_derive_ret = false;
    TEST_ASSERT_EQUAL(send(P1_FIRST_CHUNK), SWO_INCORRECT_DATA);
    TEST_ASSERT_NULL(g_message);
    TEST_ASSERT_EQUAL(g_delete_calls, 1);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    Mocktlv_apdu_Init();
    tlv_from_apdu_StubWithCallback(tlv_from_apdu_stub);
    reset();
}

void tearDown(void) {
    forget_the_message();
    Mocktlv_apdu_Verify();
    Mocktlv_apdu_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wrong_p2_rejected);
    RUN_TEST(test_unknown_p1_rejected);
    RUN_TEST(test_v1_session_refused);
    RUN_TEST(test_no_session_refused);
    RUN_TEST(test_wrong_app_state_refused);
    RUN_TEST(test_missing_schema_domain_or_message_refused);
    RUN_TEST(test_tlv_failure_is_incorrect_data);
    RUN_TEST(test_pending_chunk_succeeds_without_storing);
    RUN_TEST(test_second_message_info_refused);
    RUN_TEST(test_info_for_the_message_accepted_and_kept);
    RUN_TEST(test_receive_failure_stores_nothing);
    RUN_TEST(test_other_type_rejected_and_released);
    RUN_TEST(test_type_hash_failure_or_no_message_rejected);
    RUN_TEST(test_other_domain_rejected_and_released);
    RUN_TEST(test_domain_separator_failure_rejected);
    RUN_TEST(test_domain_with_neither_chain_nor_contract_accepted);
    RUN_TEST(test_the_contract_of_the_domain_is_handed_over);
    RUN_TEST(test_no_contract_is_handed_over_when_the_domain_has_none);
    RUN_TEST(test_no_contract_is_handed_over_when_the_domain_one_is_not_an_address);
    RUN_TEST(test_the_chain_of_the_domain_is_handed_over);
    RUN_TEST(test_no_chain_is_handed_over_when_the_domain_has_none);
    RUN_TEST(test_no_chain_is_handed_over_when_the_domain_one_is_not_a_chain);
    RUN_TEST(test_the_signer_is_the_account_of_the_signing_path);
    RUN_TEST(test_message_that_cannot_be_made_is_refused);
    RUN_TEST(test_message_without_a_signer_is_refused);
    return UNITY_END();
}
