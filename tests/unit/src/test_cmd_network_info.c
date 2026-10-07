/**
 * @file test_cmd_network_info.c
 * @brief Unit tests for handle_network_info at
 *        src/features/provide_network_info/cmd_network_info.c.
 *
 * cmd_network_info multiplexes three sub-commands behind P2:
 *
 *   P2 = NETWORK_CONFIG (0x00) -- host streams a TLV-encoded network
 *        descriptor (name, chain_id, ticker, icon hash). The Ethereum
 *        app appends it to g_dynamic_network_list. A failed parse
 *        triggers network_info_cleanup() on the partially-added node.
 *
 *   P2 = NETWORK_ICON   (0x01) -- host streams the icon blob; handled
 *        by network_icon.c. Errors trigger cleanup on the same node.
 *
 *   P2 = GET_INFO       (0x02) -- host queries the current registry:
 *        the device fills G_io_tx_buffer with `<n_networks>` then
 *        `<chain_id_be>` for each registered network (sentinel
 *        chain_id == 0 entries are skipped) and bails out if the
 *        output buffer is about to overflow.
 *
 *   default            -- SWO_WRONG_P1_P2 plus cleanup, so an unknown
 *        P2 cannot leave a half-registered network behind.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "Mocktlv_apdu.h"
#include "cmd_network_info.h"
#include "network_info.h"
#include "network_icon.h"
#include "read.h"
static uint16_t g_handle_network_icon_chunks_ret = 0x9000;

// =============================================================================
// Wraps
// =============================================================================

uint16_t handle_network_icon_chunks(uint8_t p1, const buffer_t *buf) {
    (void) p1;
    (void) buf;
    return (uint16_t) g_handle_network_icon_chunks_ret;
}

static int g_cleanup_calls = 0;
static network_info_t *g_cleanup_last_arg = NULL;
void network_info_cleanup(network_info_t *network) {
    g_cleanup_calls++;
    g_cleanup_last_arg = network;
}

// Address-only reference: cmd_network_info passes &handle_network_tlv_payload
// to tlv_from_apdu. Since tlv_from_apdu is wrapped, the callback is never
// invoked -- but the linker still needs the symbol to resolve.
bool handle_network_tlv_payload(const buffer_t *buf) {
    (void) buf;
    return true;
}

// =============================================================================
// Local tlv_from_apdu stub
// =============================================================================

static bool s_tlv_first_chunk = false;
static uint8_t s_tlv_lc = 0;
static e_tlv_apdu_ret s_tlv_ret = TLV_APDU_SUCCESS;

static e_tlv_apdu_ret tlv_from_apdu_stub(bool first_chunk,
                                         uint8_t lc,
                                         const uint8_t *payload,
                                         f_tlv_payload_handler handler,
                                         int cmock_num_calls) {
    (void) payload;
    (void) handler;
    (void) cmock_num_calls;
    s_tlv_first_chunk = first_chunk;
    s_tlv_lc = lc;
    return s_tlv_ret;
}

// =============================================================================
// Fixture
// =============================================================================

extern network_info_t *g_dynamic_network_list;  // mocks/app_globals.c
extern network_info_t *g_last_added_network;    // mocks/app_globals.c

static void reset(void) {
    g_dynamic_network_list = NULL;
    g_last_added_network = NULL;
    s_tlv_first_chunk = false;
    s_tlv_lc = 0;
    s_tlv_ret = TLV_APDU_SUCCESS;
    g_cleanup_calls = 0;
    g_cleanup_last_arg = NULL;
    memset(G_io_tx_buffer, 0, sizeof(G_io_tx_buffer));
}

// =============================================================================
// P2 = NETWORK_CONFIG
// =============================================================================

void test_network_config_tlv_success_returns_success(void) {
    unsigned int tx = 0;
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_network_info(P1_FIRST_CHUNK, /*p2*/ 0x00, (uint8_t *) "", 32, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(g_cleanup_calls, 0);
    TEST_ASSERT_TRUE(s_tlv_first_chunk);
    TEST_ASSERT_EQUAL(s_tlv_lc, 32);
}

void test_network_config_tlv_failure_cleans_up(void) {
    unsigned int tx = 0;
    // Simulate a half-added network so cleanup has a non-NULL arg to
    // free -- this proves the cleanup call passes the right node.
    network_info_t partial = {0};
    g_last_added_network = &partial;
    s_tlv_ret = TLV_APDU_ERROR;
    uint16_t sw = handle_network_info(P1_FOLLOWING_CHUNK, 0x00, (uint8_t *) "", 32, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    TEST_ASSERT_EQUAL(g_cleanup_calls, 1);
    TEST_ASSERT_EQUAL_PTR(g_cleanup_last_arg, &partial);
}

// =============================================================================
// P2 = NETWORK_ICON
// =============================================================================

void test_network_icon_success_returns_success(void) {
    unsigned int tx = 0;
    g_handle_network_icon_chunks_ret = SWO_SUCCESS;
    uint16_t sw = handle_network_info(P1_FIRST_CHUNK, /*p2*/ 0x01, (uint8_t *) "", 32, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(g_cleanup_calls, 0);
}

void test_network_icon_failure_cleans_up(void) {
    unsigned int tx = 0;
    network_info_t partial = {0};
    g_last_added_network = &partial;
    g_handle_network_icon_chunks_ret = SWO_INCORRECT_DATA;
    uint16_t sw = handle_network_info(P1_FOLLOWING_CHUNK, 0x01, (uint8_t *) "", 32, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    TEST_ASSERT_EQUAL(g_cleanup_calls, 1);
}

// =============================================================================
// P2 = GET_INFO
// =============================================================================

void test_get_info_invalid_p1_rejected(void) {
    unsigned int tx = 0;
    network_info_t partial = {0};
    g_last_added_network = &partial;
    uint16_t sw = handle_network_info(/*p1*/ 0xFF, /*p2*/ 0x02, (uint8_t *) "", 0, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_WRONG_P1_P2);
    TEST_ASSERT_EQUAL(g_cleanup_calls, 1);
    TEST_ASSERT_EQUAL_PTR(g_cleanup_last_arg, &partial);
}

void test_get_info_invalid_p1_no_in_flight_no_cleanup(void) {
    unsigned int tx = 0;
    // g_last_added_network == NULL: no in-flight network to free.
    // cleanup(NULL) would wipe the entire dynamic list -- must not be called.
    uint16_t sw = handle_network_info(/*p1*/ 0xFF, /*p2*/ 0x02, (uint8_t *) "", 0, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_WRONG_P1_P2);
    TEST_ASSERT_EQUAL(g_cleanup_calls, 0);
}

void test_get_info_empty_list_returns_zero_count(void) {
    unsigned int tx = 0;
    g_dynamic_network_list = NULL;
    uint16_t sw = handle_network_info(/*p1*/ 0x00, /*p2*/ 0x02, (uint8_t *) "", 0, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(tx, 1);  // just the count byte
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 0);
}

void test_get_info_single_network_writes_chain_id_be(void) {
    unsigned int tx = 0;
    network_info_t n = {0};
    n.chain_id = 137;  // Polygon
    g_dynamic_network_list = &n;
    uint16_t sw = handle_network_info(0x00, 0x02, (uint8_t *) "", 0, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(tx, 1 + 8);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 1);
    // chain_id encoded big-endian at offset 1.
    TEST_ASSERT_EQUAL(read_u64_be(G_io_tx_buffer, 1), 137);
}

void test_get_info_walks_multi_network_list(void) {
    unsigned int tx = 0;
    network_info_t a = {0};
    a.chain_id = 1;
    network_info_t b = {0};
    b.chain_id = 137;
    network_info_t c = {0};
    c.chain_id = 8453;  // Base
    a.node.next = (flist_node_t *) &b;
    b.node.next = (flist_node_t *) &c;
    g_dynamic_network_list = &a;
    uint16_t sw = handle_network_info(0x00, 0x02, (uint8_t *) "", 0, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(tx, 1 + 3 * 8);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 3);
    TEST_ASSERT_EQUAL(read_u64_be(G_io_tx_buffer, 1), 1);
    TEST_ASSERT_EQUAL(read_u64_be(G_io_tx_buffer, 9), 137);
    TEST_ASSERT_EQUAL(read_u64_be(G_io_tx_buffer, 17), 8453);
}

void test_get_info_breaks_when_buffer_would_overflow(void) {
    // G_io_tx_buffer has OS_IO_BUFFER_SIZE + 1 bytes. 1 count byte
    // plus N chain-IDs of 8 bytes each must fit: N = OS_IO_BUFFER_SIZE/8.
    // The (N+1)th iteration trips the overflow check. Without this guard a
    // long dynamic-network list could overrun the APDU response buffer (CWE-787).
#define OVERFLOW_MAX_NETS (OS_IO_BUFFER_SIZE / 8)
#define OVERFLOW_NODE_CNT (OVERFLOW_MAX_NETS + 2)
    unsigned int tx = 0;
    static network_info_t nodes[OVERFLOW_NODE_CNT];
    memset(nodes, 0, sizeof(nodes));
    for (int i = 0; i < OVERFLOW_NODE_CNT; i++) {
        nodes[i].chain_id = (uint64_t) (i + 1);
        if (i + 1 < OVERFLOW_NODE_CNT) {
            nodes[i].node.next = (flist_node_t *) &nodes[i + 1];
        }
    }
    g_dynamic_network_list = &nodes[0];
    uint16_t sw = handle_network_info(0x00, 0x02, (uint8_t *) "", 0, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], OVERFLOW_MAX_NETS);
    TEST_ASSERT_EQUAL(tx, 1 + OVERFLOW_MAX_NETS * 8);
#undef OVERFLOW_MAX_NETS
#undef OVERFLOW_NODE_CNT
}

void test_get_info_skips_zero_chain_id_sentinel(void) {
    // chain_id == 0 means "registration not bound to a specific chain"; the
    // dynamic-list walk skips it. The count must reflect only real entries.
    unsigned int tx = 0;
    network_info_t a = {0};
    a.chain_id = 1;
    network_info_t z = {0};
    z.chain_id = 0;  // sentinel
    network_info_t b = {0};
    b.chain_id = 137;
    a.node.next = (flist_node_t *) &z;
    z.node.next = (flist_node_t *) &b;
    g_dynamic_network_list = &a;
    uint16_t sw = handle_network_info(0x00, 0x02, (uint8_t *) "", 0, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(tx, 1 + 2 * 8);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 2);
    TEST_ASSERT_EQUAL(read_u64_be(G_io_tx_buffer, 1), 1);
    TEST_ASSERT_EQUAL(read_u64_be(G_io_tx_buffer, 9), 137);
}

// =============================================================================
// default branch -- unknown P2
// =============================================================================

void test_unknown_p2_returns_wrong_p1_p2_and_cleans_up(void) {
    unsigned int tx = 0;
    network_info_t partial = {0};
    g_last_added_network = &partial;
    uint16_t sw = handle_network_info(P1_FIRST_CHUNK, /*p2*/ 0xEE, (uint8_t *) "", 32, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_WRONG_P1_P2);
    TEST_ASSERT_EQUAL(g_cleanup_calls, 1);
}

void setUp(void) {
    Mocktlv_apdu_Init();
    tlv_from_apdu_StubWithCallback(tlv_from_apdu_stub);
    reset();
}
void tearDown(void) {
    Mocktlv_apdu_Verify();
    Mocktlv_apdu_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_network_config_tlv_success_returns_success);
    RUN_TEST(test_network_config_tlv_failure_cleans_up);
    RUN_TEST(test_network_icon_success_returns_success);
    RUN_TEST(test_network_icon_failure_cleans_up);
    RUN_TEST(test_get_info_invalid_p1_rejected);
    RUN_TEST(test_get_info_invalid_p1_no_in_flight_no_cleanup);
    RUN_TEST(test_get_info_empty_list_returns_zero_count);
    RUN_TEST(test_get_info_single_network_writes_chain_id_be);
    RUN_TEST(test_get_info_walks_multi_network_list);
    RUN_TEST(test_get_info_breaks_when_buffer_would_overflow);
    RUN_TEST(test_get_info_skips_zero_chain_id_sentinel);
    RUN_TEST(test_unknown_p2_returns_wrong_p1_p2_and_cleans_up);
    return UNITY_END();
}
