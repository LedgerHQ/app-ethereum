/**
 * @file test_eth_plugin_handler.c
 * @brief Unit tests for the plugin-handler orchestrator at
 *        src/plugins/eth_plugin_handler.c.
 *
 * This slice covers the safer entry points:
 *   - the six eth_plugin_prepare_* helpers that initialise the per-
 *     message structs the dispatcher hands to each plugin,
 *   - the chain_id mismatch defense inside
 *     eth_plugin_perform_init_default(): when the plugin was
 *     registered for a different chain than the tx is signing on,
 *     pluginStatus must be set to UNAVAILABLE without falling
 *     through to the contract / selector memcmp paths (which refuse
 *     the transaction on mismatch).
 *
 * The full dispatcher in eth_plugin_call is out of scope here — it
 * pulls every internal plugin and the SDK exception macros (BEGIN_TRY
 * / TRY / CATCH_OTHER). It is exercised end-to-end by
 * test_erc20_plugin from the plugin's own perspective.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "eth_plugin_handler.h"
#include "eth_plugin_internal.h"
#include "eth_plugin_interface.h"
#include "erc20_plugin.h"
#include "eip7002_plugin.h"
#include "eip7251_plugin.h"
#include "shared_context.h"
#include "wraps.h"
#include "Mocknetwork.h"

// =============================================================================
// Globals
// =============================================================================

// =============================================================================
// Network mock state
// =============================================================================

static uint64_t s_tx_chain_id = 1;
static uint64_t get_tx_chain_id_stub(int cmock_num_calls) {
    (void) cmock_num_calls;
    return s_tx_chain_id;
}

static const char *s_displayable_ticker = "ETH";
static const char *get_displayable_ticker_stub(const uint64_t *chain_id,
                                               const chain_config_t *chain_cfg,
                                               bool dynamic,
                                               int cmock_num_calls) {
    (void) chain_id;
    (void) chain_cfg;
    (void) dynamic;
    (void) cmock_num_calls;
    return s_displayable_ticker;
}

// =============================================================================
// Wraps
// =============================================================================

static union extraInfo_t *g_asset_info_ret = NULL;
union extraInfo_t *get_matching_asset_info(const uint64_t *chain_id, const uint8_t *address) {
    (void) chain_id;
    (void) address;
    return g_asset_info_ret;
}

// =============================================================================
// Stubs for internal plugin _call functions. The dispatch tests below
// configure the result value the stub writes back via msg->result so
// every plugin call site is observable from the test.
// =============================================================================
static eth_plugin_result_t g_plugin_result = ETH_PLUGIN_RESULT_OK;
static int g_erc20_calls = 0;
static int g_erc721_calls = 0;
static int g_erc1155_calls = 0;
static int g_swap_with_calldata_calls = 0;

static void set_plugin_result(eth_plugin_msg_t message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT:
            ((ethPluginInitContract_t *) parameters)->result = g_plugin_result;
            break;
        case ETH_PLUGIN_PROVIDE_PARAMETER:
            ((ethPluginProvideParameter_t *) parameters)->result = g_plugin_result;
            break;
        case ETH_PLUGIN_FINALIZE:
            ((ethPluginFinalize_t *) parameters)->result = g_plugin_result;
            break;
        case ETH_PLUGIN_PROVIDE_INFO:
            ((ethPluginProvideInfo_t *) parameters)->result = g_plugin_result;
            break;
        case ETH_PLUGIN_QUERY_CONTRACT_ID:
            ((ethQueryContractID_t *) parameters)->result = g_plugin_result;
            break;
        case ETH_PLUGIN_QUERY_CONTRACT_UI:
            ((ethQueryContractUI_t *) parameters)->result = g_plugin_result;
            break;
        default:
            break;
    }
}

void erc20_plugin_call(eth_plugin_msg_t message, void *parameters) {
    g_erc20_calls++;
    set_plugin_result(message, parameters);
}
void eth2_plugin_call(eth_plugin_msg_t message, void *parameters) {
    (void) message;
    (void) parameters;
}
void erc721_plugin_call(eth_plugin_msg_t message, void *parameters) {
    g_erc721_calls++;
    set_plugin_result(message, parameters);
}
void erc1155_plugin_call(eth_plugin_msg_t message, void *parameters) {
    g_erc1155_calls++;
    set_plugin_result(message, parameters);
}
void swap_with_calldata_plugin_call(eth_plugin_msg_t message, void *parameters) {
    g_swap_with_calldata_calls++;
    set_plugin_result(message, parameters);
}
void eip7002_plugin_call(eth_plugin_msg_t message, void *parameters) {
    (void) message;
    (void) parameters;
}
void eip7251_plugin_call(eth_plugin_msg_t message, void *parameters) {
    (void) message;
    (void) parameters;
}

// app_exit is noreturn. Most tests must NEVER reach it (silent
// fall-through to app_exit would be a security bug). The defensive-
// guard tests below set g_expect_app_exit + a jmp_buf so the noreturn
// contract can be unwound back to the test body.
static bool g_expect_app_exit = false;
static bool g_app_exit_reached = false;
static jmp_buf g_app_exit_jmp;
__attribute__((noreturn)) void app_exit(void) {
    if (g_expect_app_exit) {
        g_app_exit_reached = true;
        longjmp(g_app_exit_jmp, 1);
    }
    TEST_FAIL_MESSAGE("app_exit() reached unexpectedly");
    while (1) {
    }
}

// os_longjmp is overridden locally (vs. the silent stub in mocks/mock.c)
// to make any unexpected exception loud — the external-plugin path is
// not exercised by these tests, so reaching it would be a bug.
__attribute__((noreturn)) void os_longjmp(unsigned int e) {
    (void) e;
    TEST_FAIL_MESSAGE("os_longjmp() reached unexpectedly");
    while (1) {
    }
}

// Plugin selectors / addresses referenced by INTERNAL_ETH_PLUGINS.
// Production code links these against the actual plugin .c files; here
// we provide concrete byte arrays so the matching_selector /
// matching_address loops inside eth_plugin_perform_init_old_internal
// don't memcmp against NULL pointers (UB).
static const uint8_t g_erc20_transfer_selector[4] = {0xA9, 0x05, 0x9C, 0xBB};
static const uint8_t g_erc20_approve_selector[4] = {0x09, 0x5E, 0xA7, 0xB3};
const uint8_t *const ERC20_SELECTORS[NUM_ERC20_SELECTORS] = {
    g_erc20_transfer_selector,
    g_erc20_approve_selector,
};
#ifdef HAVE_ETH2
static const uint8_t g_eth2_selector[4] = {0x22, 0x89, 0x51, 0x18};
static const uint8_t g_eth2_address[ADDRESS_LENGTH] = {0xE2};
const uint8_t *const ETH2_SELECTORS[NUM_ETH2_SELECTORS] = {g_eth2_selector};
const uint8_t *const ETH2_ADDRESSES[NUM_ETH2_ADDRESSES] = {g_eth2_address};
#endif
static const uint8_t g_eip7002_address[ADDRESS_LENGTH] = {0x70};
static const uint8_t g_eip7251_address[ADDRESS_LENGTH] = {0x72};
const uint8_t *const EIP7002_ADDRESSES[NUM_EIP7002_ADDRESSES] = {g_eip7002_address};
const uint8_t *const EIP7251_ADDRESSES[NUM_EIP7251_ADDRESSES] = {g_eip7251_address};

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    memset(&dataContext, 0, sizeof(dataContext));
    memset(&tmpCtx, 0, sizeof(tmpCtx));
    memset(&tmpContent, 0, sizeof(tmpContent));
    pluginType = PLUGIN_TYPE_NONE;
    g_asset_info_ret = NULL;
    s_tx_chain_id = 1;
    s_displayable_ticker = "ETH";
    g_plugin_result = ETH_PLUGIN_RESULT_OK;
    g_erc20_calls = 0;
    g_erc721_calls = 0;
    g_erc1155_calls = 0;
    g_swap_with_calldata_calls = 0;
    // Default: cached plugin available, so eth_plugin_call doesn't bail
    // on the "cached but unavailable" early-return at the top of the
    // function.
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_OK;
    g_expect_app_exit = false;
    g_app_exit_reached = false;
}

// =============================================================================
// eth_plugin_prepare_* helpers
// =============================================================================

void test_prepare_init_populates_struct(void) {
    ethPluginInitContract_t init;
    memset(&init, 0xFF, sizeof(init));  // pre-pollute
    const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};

    eth_plugin_prepare_init(&init, selector, 1234);
    TEST_ASSERT_EQUAL_PTR(init.selector, selector);
    TEST_ASSERT_EQUAL(init.dataSize, 1234);
    // Other fields should be zeroed by the explicit_bzero up front.
    TEST_ASSERT_EQUAL(init.result, 0);
    TEST_ASSERT_NULL(init.bip32);
}

void test_prepare_provide_parameter_populates_struct(void) {
    ethPluginProvideParameter_t msg;
    memset(&msg, 0xFF, sizeof(msg));
    uint8_t param[32] = {0xAA};

    eth_plugin_prepare_provide_parameter(&msg, param, 36, 32);
    TEST_ASSERT_EQUAL_PTR(msg.parameter, param);
    TEST_ASSERT_EQUAL(msg.parameterOffset, 36);
    TEST_ASSERT_EQUAL(msg.parameter_size, 32);
    TEST_ASSERT_EQUAL(msg.result, 0);
}

void test_prepare_finalize_zeroes_struct(void) {
    ethPluginFinalize_t msg;
    memset(&msg, 0xFF, sizeof(msg));

    eth_plugin_prepare_finalize(&msg);
    TEST_ASSERT_NULL(msg.tokenLookup1);
    TEST_ASSERT_NULL(msg.tokenLookup2);
    TEST_ASSERT_EQUAL(msg.result, 0);
    TEST_ASSERT_EQUAL(msg.uiType, 0);
}

void test_prepare_provide_info_zeroes_struct(void) {
    ethPluginProvideInfo_t msg;
    memset(&msg, 0xFF, sizeof(msg));

    eth_plugin_prepare_provide_info(&msg);
    TEST_ASSERT_NULL(msg.item1);
    TEST_ASSERT_NULL(msg.item2);
    TEST_ASSERT_EQUAL(msg.result, 0);
}

void test_prepare_query_contract_id_populates_struct(void) {
    ethQueryContractID_t msg;
    memset(&msg, 0xFF, sizeof(msg));
    char name[16], version[16];

    eth_plugin_prepare_query_contract_id(&msg, name, sizeof(name), version, sizeof(version));
    TEST_ASSERT_EQUAL_PTR(msg.name, name);
    TEST_ASSERT_EQUAL(msg.nameLength, sizeof(name));
    TEST_ASSERT_EQUAL_PTR(msg.version, version);
    TEST_ASSERT_EQUAL(msg.versionLength, sizeof(version));
    TEST_ASSERT_EQUAL(msg.result, 0);
}

void test_prepare_query_contract_ui_resolves_chain_and_assets(void) {
    ethQueryContractUI_t msg;
    memset(&msg, 0xFF, sizeof(msg));
    char title[16], msg_buf[64];

    s_tx_chain_id = 137;  // Polygon
    s_displayable_ticker = "POL";
    static union extraInfo_t fake1;
    g_asset_info_ret = &fake1;  // both lookup calls return the same pointer

    eth_plugin_prepare_query_contract_ui(&msg, 2, title, sizeof(title), msg_buf, sizeof(msg_buf));
    TEST_ASSERT_EQUAL(msg.screenIndex, 2);
    TEST_ASSERT_EQUAL_PTR(msg.title, title);
    TEST_ASSERT_EQUAL_PTR(msg.msg, msg_buf);
    TEST_ASSERT_EQUAL(msg.titleLength, sizeof(title));
    TEST_ASSERT_EQUAL(msg.msgLength, sizeof(msg_buf));
    TEST_ASSERT_EQUAL_STRING(msg.network_ticker, "POL");
    TEST_ASSERT_EQUAL_PTR(msg.item1, &fake1);
    TEST_ASSERT_EQUAL_PTR(msg.item2, &fake1);
}

// =============================================================================
// eth_plugin_perform_init — chain_id mismatch defense
// =============================================================================

void test_perform_init_default_chain_mismatch_marks_unavailable(void) {
    // External plugin registered for chain 1, transaction is on chain 137.
    // The mismatch path must set pluginStatus = UNAVAILABLE and return
    // WITHOUT reaching the contract / selector memcmp branches (which
    // would abort the app on disagreement).
    dataContext.tokenContext.pluginChainId = 1;  // not PLUGIN_CHAIN_ID_ANY
    s_tx_chain_id = 137;
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_OK;  // pre-existing
    pluginType = PLUGIN_TYPE_EXTERNAL;

    uint8_t contract[ADDRESS_LENGTH] = {0xAA};
    const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    ethPluginInitContract_t init = {.selector = selector};
    eth_plugin_perform_init(contract, &init);

    TEST_ASSERT_EQUAL(dataContext.tokenContext.pluginStatus, ETH_PLUGIN_RESULT_UNAVAILABLE);
}

void test_perform_init_default_chain_any_bypasses_check(void) {
    // When pluginChainId == PLUGIN_CHAIN_ID_ANY (0), the mismatch
    // check is skipped — the registration is chain-unbound. The contract
    // matches what's in tokenContext, so the path proceeds to the eth_
    // plugin_call which our stub leaves with the default UNAVAILABLE
    // status — that's fine for this assertion.
    dataContext.tokenContext.pluginChainId = PLUGIN_CHAIN_ID_ANY;
    s_tx_chain_id = 137;
    pluginType = PLUGIN_TYPE_EXTERNAL;

    uint8_t contract[ADDRESS_LENGTH] = {0xAA};
    memcpy(dataContext.tokenContext.contractAddress, contract, ADDRESS_LENGTH);
    const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    memcpy(dataContext.tokenContext.methodSelector, selector, CALLDATA_SELECTOR_SIZE);
    ethPluginInitContract_t init = {.selector = selector};
    // Just checking the call doesn't abort via app_exit — return value
    // depends on the (unstubbed) external plugin call.
    eth_plugin_perform_init(contract, &init);
}

void test_perform_init_default_unresolved_tx_chain_defers_check(void) {
    // When tx_chain_id == 0 (LEGACY tx, chain_id only known after V is
    // parsed), the mismatch check is deferred. The path falls through
    // to the contract/selector match.
    dataContext.tokenContext.pluginChainId = 1;
    s_tx_chain_id = 0;  // unresolved
    pluginType = PLUGIN_TYPE_EXTERNAL;

    uint8_t contract[ADDRESS_LENGTH] = {0xAA};
    memcpy(dataContext.tokenContext.contractAddress, contract, ADDRESS_LENGTH);
    const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    memcpy(dataContext.tokenContext.methodSelector, selector, CALLDATA_SELECTOR_SIZE);
    ethPluginInitContract_t init = {.selector = selector};
    eth_plugin_perform_init(contract, &init);

    // Did not get marked UNAVAILABLE by the chain check — it was deferred.
    // (The contract/selector match succeeded so the OK status is set
    // before dispatching to the external plugin.)
    TEST_ASSERT_EQUAL(dataContext.tokenContext.pluginStatus, ETH_PLUGIN_RESULT_OK);
}

// =============================================================================
// Registration / transaction disagreement — these guards are the *last
// line of defense* against state-corruption between the plugin INIT
// phase and the eth_plugin_perform_init() entry. A silent fall-through
// would let a registered plugin be invoked on the wrong contract /
// selector, so we confirm they refuse the transaction: ERROR status,
// no plugin dispatched, and the app still alive to answer the host.
// =============================================================================

void test_perform_init_default_contract_mismatch_refuses(void) {
    // Plugin was registered for contract A; the transaction now claims
    // contract B. The parsing must be refused — anything else lets the
    // wrong plugin take over the rendering.
    dataContext.tokenContext.pluginChainId = PLUGIN_CHAIN_ID_ANY;
    pluginType = PLUGIN_TYPE_EXTERNAL;
    const uint8_t registered_contract[ADDRESS_LENGTH] = {[0] = 0xAA};
    const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    memcpy(dataContext.tokenContext.contractAddress, registered_contract, ADDRESS_LENGTH);
    memcpy(dataContext.tokenContext.methodSelector, selector, CALLDATA_SELECTOR_SIZE);

    uint8_t observed_contract[ADDRESS_LENGTH] = {[0] = 0xBB};  // different
    ethPluginInitContract_t init = {.selector = selector};

    eth_plugin_result_t r = eth_plugin_perform_init(observed_contract, &init);

    TEST_ASSERT_EQUAL(ETH_PLUGIN_RESULT_ERROR, r);
    TEST_ASSERT_EQUAL(ETH_PLUGIN_RESULT_ERROR, dataContext.tokenContext.pluginStatus);
    TEST_ASSERT_EQUAL(0, g_erc20_calls);
}

void test_perform_init_default_selector_mismatch_refuses(void) {
    // Plugin registered for selector X; tx now uses selector Y.
    dataContext.tokenContext.pluginChainId = PLUGIN_CHAIN_ID_ANY;
    pluginType = PLUGIN_TYPE_EXTERNAL;
    uint8_t contract[ADDRESS_LENGTH] = {[0] = 0xAA};
    const uint8_t registered_selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    memcpy(dataContext.tokenContext.contractAddress, contract, ADDRESS_LENGTH);
    memcpy(dataContext.tokenContext.methodSelector, registered_selector, CALLDATA_SELECTOR_SIZE);

    const uint8_t observed_selector[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    ethPluginInitContract_t init = {.selector = observed_selector};

    eth_plugin_result_t r = eth_plugin_perform_init(contract, &init);

    TEST_ASSERT_EQUAL(ETH_PLUGIN_RESULT_ERROR, r);
    TEST_ASSERT_EQUAL(ETH_PLUGIN_RESULT_ERROR, dataContext.tokenContext.pluginStatus);
    TEST_ASSERT_EQUAL(0, g_erc20_calls);
}

void test_perform_init_default_double_init_refuses(void) {
    // PLUGIN_CTX_PLUGIN means INIT_CONTRACT already ran and the union no
    // longer holds the address/selector the memcmp below would read.
    dataContext.tokenContext.pluginChainId = PLUGIN_CHAIN_ID_ANY;
    dataContext.tokenContext.plugin_ctx_mode = PLUGIN_CTX_PLUGIN;
    pluginType = PLUGIN_TYPE_EXTERNAL;

    uint8_t contract[ADDRESS_LENGTH] = {[0] = 0xAA};
    const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    ethPluginInitContract_t init = {.selector = selector};

    eth_plugin_result_t r = eth_plugin_perform_init(contract, &init);

    TEST_ASSERT_EQUAL(ETH_PLUGIN_RESULT_ERROR, r);
    TEST_ASSERT_EQUAL(ETH_PLUGIN_RESULT_ERROR, dataContext.tokenContext.pluginStatus);
    TEST_ASSERT_EQUAL(0, g_erc20_calls);
}

void test_perform_init_unsupported_plugin_type_panics(void) {
    // pluginType outside every case → default branch must app_exit.
    pluginType = (pluginType_t) 0x7F;  // arbitrary unknown value

    uint8_t contract[ADDRESS_LENGTH] = {0};
    const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0};
    ethPluginInitContract_t init = {.selector = selector};

    g_expect_app_exit = true;
    if (setjmp(g_app_exit_jmp) == 0) {
        eth_plugin_perform_init(contract, &init);
        TEST_FAIL_MESSAGE("app_exit not reached on unsupported pluginType");
    }
    TEST_ASSERT_TRUE(g_app_exit_reached);
}

// =============================================================================
// eth_plugin_call — dispatcher per pluginType + per method + result check
// =============================================================================

void test_call_cached_unavailable_short_circuits(void) {
    // When the cached pluginStatus is UNAVAILABLE, the call must return
    // that status immediately without dispatching to any plugin.
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;
    ethPluginInitContract_t msg = {0};
    eth_plugin_result_t r = eth_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(r, ETH_PLUGIN_RESULT_UNAVAILABLE);
    TEST_ASSERT_EQUAL(g_erc20_calls, 0);
}

void test_call_unknown_method_returns_unavailable(void) {
    pluginType = PLUGIN_TYPE_OLD_INTERNAL;
    // Some method outside the switch cases.
    eth_plugin_result_t r = eth_plugin_call((eth_plugin_msg_t) 0xFFFF, NULL);
    TEST_ASSERT_EQUAL(r, ETH_PLUGIN_RESULT_UNAVAILABLE);
}

void test_call_erc721_dispatches(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    ethPluginInitContract_t msg = {0};
    eth_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(g_erc721_calls, 1);
    TEST_ASSERT_EQUAL(g_erc20_calls, 0);
}

void test_call_erc1155_dispatches(void) {
    pluginType = PLUGIN_TYPE_ERC1155;
    ethPluginInitContract_t msg = {0};
    eth_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(g_erc1155_calls, 1);
}

void test_call_swap_with_calldata_dispatches(void) {
    pluginType = PLUGIN_TYPE_SWAP_WITH_CALLDATA;
    ethPluginInitContract_t msg = {0};
    eth_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(g_swap_with_calldata_calls, 1);
}

void test_call_old_internal_routes_by_alias(void) {
    pluginType = PLUGIN_TYPE_OLD_INTERNAL;
    strlcpy(dataContext.tokenContext.pluginName,
            "-erc20",
            sizeof(dataContext.tokenContext.pluginName));
    ethPluginInitContract_t msg = {0};
    eth_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(g_erc20_calls, 1);
}

void test_call_unsupported_plugin_type_returns_error(void) {
    pluginType = (pluginType_t) 0xFF;
    ethPluginInitContract_t msg = {0};
    eth_plugin_result_t r = eth_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(r, ETH_PLUGIN_RESULT_ERROR);
}

// --- Result-check matrix ---

void test_call_init_error_result_propagates(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    g_plugin_result = ETH_PLUGIN_RESULT_ERROR;
    ethPluginInitContract_t msg = {0};
    eth_plugin_result_t r = eth_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(r, ETH_PLUGIN_RESULT_ERROR);
}

void test_call_init_unknown_result_returns_unavailable(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    g_plugin_result = ETH_PLUGIN_RESULT_FALLBACK;  // not OK/ERROR for INIT
    ethPluginInitContract_t msg = {0};
    eth_plugin_result_t r = eth_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(r, ETH_PLUGIN_RESULT_UNAVAILABLE);
}

void test_call_provide_parameter_fallback_passes(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    g_plugin_result = ETH_PLUGIN_RESULT_FALLBACK;
    ethPluginProvideParameter_t msg = {0};
    // For PROVIDE_PARAMETER, FALLBACK is accepted as OK by the
    // dispatcher (it's treated the same as OK at the return point).
    eth_plugin_result_t r = eth_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(r, ETH_PLUGIN_RESULT_OK);
}

void test_call_finalize_ok(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    ethPluginFinalize_t msg = {0};
    eth_plugin_result_t r = eth_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(r, ETH_PLUGIN_RESULT_OK);
}

void test_call_provide_info_error_propagates(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    g_plugin_result = ETH_PLUGIN_RESULT_ERROR;
    ethPluginProvideInfo_t msg = {0};
    eth_plugin_result_t r = eth_plugin_call(ETH_PLUGIN_PROVIDE_INFO, &msg);
    TEST_ASSERT_EQUAL(r, ETH_PLUGIN_RESULT_ERROR);
}

void test_call_query_contract_id_error_propagates(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    g_plugin_result = ETH_PLUGIN_RESULT_ERROR;
    ethQueryContractID_t msg = {0};
    eth_plugin_result_t r = eth_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    TEST_ASSERT_EQUAL(r, ETH_PLUGIN_RESULT_ERROR);
}

void test_call_query_contract_ui_ok(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    g_plugin_result = ETH_PLUGIN_RESULT_OK;
    ethQueryContractUI_t msg = {0};
    eth_plugin_result_t r = eth_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(r, ETH_PLUGIN_RESULT_OK);
}

// Result-check error branches for every message type. The dispatcher
// must propagate the plugin's RESULT_ERROR as-is, and turn any other
// non-OK / non-FALLBACK value (default branch) into UNAVAILABLE.
//
// Without these tests, a regression that flipped ERROR → UNAVAILABLE
// (or vice versa) for any one message type would change the caller's
// behavior silently — INIT_CONTRACT propagates ERROR up to refuse the
// signature flow, while FINALIZE / PROVIDE_PARAMETER report fault
// back to the parser which decides whether to abort.

void test_call_provide_parameter_error_propagates(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    g_plugin_result = ETH_PLUGIN_RESULT_ERROR;
    ethPluginProvideParameter_t msg = {0};
    TEST_ASSERT_EQUAL(eth_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg), ETH_PLUGIN_RESULT_ERROR);
}

void test_call_provide_parameter_unknown_returns_unavailable(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    g_plugin_result = ETH_PLUGIN_RESULT_UNAVAILABLE;  // outside {OK, FALLBACK, ERROR}
    ethPluginProvideParameter_t msg = {0};
    TEST_ASSERT_EQUAL(eth_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg),
                      ETH_PLUGIN_RESULT_UNAVAILABLE);
}

void test_call_finalize_error_propagates(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    g_plugin_result = ETH_PLUGIN_RESULT_ERROR;
    ethPluginFinalize_t msg = {0};
    TEST_ASSERT_EQUAL(eth_plugin_call(ETH_PLUGIN_FINALIZE, &msg), ETH_PLUGIN_RESULT_ERROR);
}

void test_call_finalize_unknown_returns_unavailable(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    g_plugin_result = ETH_PLUGIN_RESULT_UNAVAILABLE;
    ethPluginFinalize_t msg = {0};
    TEST_ASSERT_EQUAL(eth_plugin_call(ETH_PLUGIN_FINALIZE, &msg), ETH_PLUGIN_RESULT_UNAVAILABLE);
}

void test_call_provide_info_unknown_returns_unavailable(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    g_plugin_result = ETH_PLUGIN_RESULT_UNAVAILABLE;
    ethPluginProvideInfo_t msg = {0};
    TEST_ASSERT_EQUAL(eth_plugin_call(ETH_PLUGIN_PROVIDE_INFO, &msg),
                      ETH_PLUGIN_RESULT_UNAVAILABLE);
}

void test_call_query_contract_ui_error_propagates(void) {
    pluginType = PLUGIN_TYPE_ERC721;
    g_plugin_result = ETH_PLUGIN_RESULT_ERROR;
    ethQueryContractUI_t msg = {0};
    TEST_ASSERT_EQUAL(eth_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg), ETH_PLUGIN_RESULT_ERROR);
}

// =============================================================================
// eth_plugin_perform_init — PLUGIN_TYPE_NONE path (old_internal lookup)
// =============================================================================

void test_perform_init_swap_with_calldata_sets_ok(void) {
    pluginType = PLUGIN_TYPE_SWAP_WITH_CALLDATA;
    uint8_t contract[ADDRESS_LENGTH] = {0xAA};
    const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    ethPluginInitContract_t init = {.selector = selector};
    eth_plugin_perform_init(contract, &init);
    // The swap_with_calldata branch immediately marks the plugin OK.
    // The function returns OK without calling the plugin's init.
    TEST_ASSERT_EQUAL(dataContext.tokenContext.pluginStatus, ETH_PLUGIN_RESULT_OK);
}

// =============================================================================
// eth_plugin_perform_init_old_internal — INTERNAL_ETH_PLUGINS lookup.
//
// The PLUGIN_TYPE_NONE path walks INTERNAL_ETH_PLUGINS trying every
// registered plugin's (address-list, selector-list) tuple. A regression
// that returned true unconditionally would let any contract / selector
// claim the ERC-20 alias — these tests pin the actual match contract.
// =============================================================================

void test_perform_init_none_matches_erc20_transfer(void) {
    // ERC-20 plugin entry has addresses=NULL (any contract) and
    // selectors={transfer, approve}. Match the first selector.
    pluginType = PLUGIN_TYPE_NONE;
    uint8_t contract[ADDRESS_LENGTH] = {0};
    contract[0] = 0xAB;
    ethPluginInitContract_t init = {.selector = g_erc20_transfer_selector};

    eth_plugin_perform_init(contract, &init);

    // The function copies the plugin alias into tokenContext.pluginName
    // and switches pluginType to OLD_INTERNAL.
    TEST_ASSERT_EQUAL(pluginType, PLUGIN_TYPE_OLD_INTERNAL);
    TEST_ASSERT_EQUAL_STRING(dataContext.tokenContext.pluginName, "-erc20");
    TEST_ASSERT_EQUAL(dataContext.tokenContext.pluginStatus, ETH_PLUGIN_RESULT_OK);
}

void test_perform_init_none_matches_eip7002_by_address(void) {
    // EIP-7002 entry has addresses={g_eip7002_address} (constraint),
    // selectors=NULL (any selector → matching_selector returns true).
    pluginType = PLUGIN_TYPE_NONE;
    uint8_t contract[ADDRESS_LENGTH];
    memcpy(contract, g_eip7002_address, ADDRESS_LENGTH);
    const uint8_t unrelated_selector[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    ethPluginInitContract_t init = {.selector = unrelated_selector};

    eth_plugin_perform_init(contract, &init);

    TEST_ASSERT_EQUAL(pluginType, PLUGIN_TYPE_OLD_INTERNAL);
    TEST_ASSERT_EQUAL_STRING(dataContext.tokenContext.pluginName, "-eip7002");
}

void test_perform_init_none_no_match_unavailable(void) {
    // Contract not in any address-bound plugin, selector not in any
    // selector-bound plugin → old_internal returns false. Then
    // contract_address != NULL and G_called_from_swap=false → returns
    // ETH_PLUGIN_RESULT_UNAVAILABLE without disturbing pluginType.
    pluginType = PLUGIN_TYPE_NONE;
    G_called_from_swap = false;
    uint8_t contract[ADDRESS_LENGTH];
    memset(contract, 0xCD, ADDRESS_LENGTH);  // matches no plugin address
    const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    ethPluginInitContract_t init = {.selector = selector};

    eth_plugin_perform_init(contract, &init);

    // Stayed NONE (no match overwrote it), default status UNAVAILABLE.
    TEST_ASSERT_EQUAL(pluginType, PLUGIN_TYPE_NONE);
    TEST_ASSERT_EQUAL(dataContext.tokenContext.pluginStatus, ETH_PLUGIN_RESULT_UNAVAILABLE);
}

void test_perform_init_none_no_match_swap_mode_returns_error(void) {
    // Same as above but under swap mode. The post-switch block must
    // reject explicitly (RESULT_ERROR) instead of returning UNAVAILABLE:
    // swap flows can't fall back to a generic "I don't know this
    // contract" rendering — the user is then signing a contract
    // call blind during an exchange and that path is unsafe.
    pluginType = PLUGIN_TYPE_NONE;
    G_called_from_swap = true;
    uint8_t contract[ADDRESS_LENGTH];
    memset(contract, 0xCD, ADDRESS_LENGTH);
    const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    ethPluginInitContract_t init = {.selector = selector};

    eth_plugin_result_t r = eth_plugin_perform_init(contract, &init);
    TEST_ASSERT_EQUAL(r, ETH_PLUGIN_RESULT_ERROR);

    // Restore for next test.
    G_called_from_swap = false;
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    Mocknetwork_Init();
    get_tx_chain_id_StubWithCallback(get_tx_chain_id_stub);
    get_displayable_ticker_StubWithCallback(get_displayable_ticker_stub);
    reset();
}
void tearDown(void) {
    Mocknetwork_Verify();
    Mocknetwork_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_prepare_init_populates_struct);
    RUN_TEST(test_prepare_provide_parameter_populates_struct);
    RUN_TEST(test_prepare_finalize_zeroes_struct);
    RUN_TEST(test_prepare_provide_info_zeroes_struct);
    RUN_TEST(test_prepare_query_contract_id_populates_struct);
    RUN_TEST(test_prepare_query_contract_ui_resolves_chain_and_assets);
    RUN_TEST(test_perform_init_default_chain_mismatch_marks_unavailable);
    RUN_TEST(test_perform_init_default_chain_any_bypasses_check);
    RUN_TEST(test_perform_init_default_unresolved_tx_chain_defers_check);
    RUN_TEST(test_perform_init_default_contract_mismatch_refuses);
    RUN_TEST(test_perform_init_default_selector_mismatch_refuses);
    RUN_TEST(test_perform_init_default_double_init_refuses);
    RUN_TEST(test_perform_init_unsupported_plugin_type_panics);
    RUN_TEST(test_call_cached_unavailable_short_circuits);
    RUN_TEST(test_call_unknown_method_returns_unavailable);
    RUN_TEST(test_call_erc721_dispatches);
    RUN_TEST(test_call_erc1155_dispatches);
    RUN_TEST(test_call_swap_with_calldata_dispatches);
    RUN_TEST(test_call_old_internal_routes_by_alias);
    RUN_TEST(test_call_unsupported_plugin_type_returns_error);
    RUN_TEST(test_call_init_error_result_propagates);
    RUN_TEST(test_call_init_unknown_result_returns_unavailable);
    RUN_TEST(test_call_provide_parameter_fallback_passes);
    RUN_TEST(test_call_finalize_ok);
    RUN_TEST(test_call_provide_info_error_propagates);
    RUN_TEST(test_call_query_contract_id_error_propagates);
    RUN_TEST(test_call_query_contract_ui_ok);
    RUN_TEST(test_call_provide_parameter_error_propagates);
    RUN_TEST(test_call_provide_parameter_unknown_returns_unavailable);
    RUN_TEST(test_call_finalize_error_propagates);
    RUN_TEST(test_call_finalize_unknown_returns_unavailable);
    RUN_TEST(test_call_provide_info_unknown_returns_unavailable);
    RUN_TEST(test_call_query_contract_ui_error_propagates);
    RUN_TEST(test_perform_init_swap_with_calldata_sets_ok);
    RUN_TEST(test_perform_init_none_matches_erc20_transfer);
    RUN_TEST(test_perform_init_none_matches_eip7002_by_address);
    RUN_TEST(test_perform_init_none_no_match_unavailable);
    RUN_TEST(test_perform_init_none_no_match_swap_mode_returns_error);
    return UNITY_END();
}
