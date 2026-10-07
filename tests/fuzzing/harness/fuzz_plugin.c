/*
 * fuzz_plugin: drives one internal plugin through the production sequence
 * (init -> parameters -> finalize -> info -> id -> ui), dispatched by
 * eth_plugin_call() so plugin selection and the shared handler code are fuzzed
 * too rather than re-implemented here.
 *
 * Control byte 1 picks the plugin row and byte 2 indexes its address/selector
 * table. Then comes this app's fixed header (mock/plugin_model.h), and after it
 * the rest of the input is fed straight through as 32-byte ABI words.
 */
#include <string.h>

#include "mocks.h"
#include "plugin_model.h"

#include "shared_context.h"
#include "eth_plugin_handler.h"
#include "eth_plugin_interface.h"
#include "plugins.h"
#include "manage_asset_info.h"
#include "eth_swap_utils.h"
#include "network.h"
#include "mem_utils.h"  // app_mem_init()
#include "erc20_plugin.h"
#include "eip7002_plugin.h"
#include "eip7251_plugin.h"
#ifdef HAVE_ETH2
#include "eth2_plugin.h"
#endif

#define FUZZ_APP_HEADER_LEN ((int) sizeof(eth_plugin_header_t))
#define FUZZ_APP_CUSTOM_ENTRY
#include "fuzz_harness.h"

/** How production selects one plugin; NULL tables mean the header decides. */
typedef struct {
    pluginType_t type;  ///< Preset before selection; NONE resolves by table match.
    const uint8_t *const *addresses;
    uint8_t num_addresses;
    const uint8_t *const *selectors;
    uint8_t num_selectors;
} eth_fuzz_plugin_t;

/* Conditional rows go last so the fixed indices below stay valid. */
static const eth_fuzz_plugin_t k_plugins[] = {
    {PLUGIN_TYPE_NONE, NULL, 0, ERC20_SELECTORS, NUM_ERC20_SELECTORS},
    {PLUGIN_TYPE_NONE, EIP7002_ADDRESSES, NUM_EIP7002_ADDRESSES, NULL, 0},
    {PLUGIN_TYPE_NONE, EIP7251_ADDRESSES, NUM_EIP7251_ADDRESSES, NULL, 0},
    {PLUGIN_TYPE_ERC721, NULL, 0, NULL, 0},
    {PLUGIN_TYPE_ERC1155, NULL, 0, NULL, 0},
    {PLUGIN_TYPE_SWAP_WITH_CALLDATA, NULL, 0, NULL, 0},
#ifdef HAVE_ETH2
    {PLUGIN_TYPE_NONE, ETH2_ADDRESSES, NUM_ETH2_ADDRESSES, ETH2_SELECTORS, NUM_ETH2_SELECTORS},
#endif
};

/* ins carries the row index so fuzz_app_dispatch() recovers the pick. */
#define PLUGIN_ROW(idx) {.ins = (idx), .flags = FUZZ_CMD_HAS_DATA}
const fuzz_command_spec_t fuzz_commands[] = {
    PLUGIN_ROW(0),
    PLUGIN_ROW(1),
    PLUGIN_ROW(2),
    PLUGIN_ROW(3),
    PLUGIN_ROW(4),
    PLUGIN_ROW(5),
#ifdef HAVE_ETH2
    PLUGIN_ROW(6),
#endif
};
FUZZ_COMMAND_COUNT();
_Static_assert(ARRAYLEN(fuzz_commands) == ARRAYLEN(k_plugins), "plugin tables out of sync");

extern const chain_config_t g_fuzz_chain_config;  // mock/app_globals.c

static eth_plugin_header_t g_hdr;

void fuzz_app_reset(void) {
    reset_app_context();
    app_mem_init();

    g_caller_app = NULL;
    // Absolution zeroes g_chain_config (zero-symbols); production keeps it set.
    g_chain_config = &g_fuzz_chain_config;
}

/** Feeds the payload as ABI words, at the offsets logic_sign_tx.c uses. */
static bool provide_parameters(const uint8_t *data, size_t size) {
    uint32_t offset = CALLDATA_SELECTOR_SIZE;

    for (size_t pos = 0; pos < size; pos += PARAMETER_LENGTH) {
        ethPluginProvideParameter_t provide;
        uint8_t word[PARAMETER_LENGTH] = {0};
        size_t left = size - pos;
        uint8_t len = (uint8_t) (left < PARAMETER_LENGTH ? left : PARAMETER_LENGTH);

        memcpy(word, data + pos, len);
        eth_plugin_prepare_provide_parameter(&provide, word, offset, len);
        if (eth_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &provide) <=
            ETH_PLUGIN_RESULT_UNSUCCESSFUL) {
            return false;
        }
        offset += PARAMETER_LENGTH;
    }
    return true;
}

/** Mirrors the token-lookup block of finalize_parsing_helper(). */
static eth_plugin_result_t provide_info(const ethPluginFinalize_t *finalize,
                                        uint8_t *additional_screens) {
    ethPluginProvideInfo_t info;
    uint64_t chain_id = get_tx_chain_id();

    dataContext.tokenContext.token_lookup1 = finalize->tokenLookup1;
    dataContext.tokenContext.token_lookup2 = finalize->tokenLookup2;
    if (finalize->tokenLookup1 == NULL && finalize->tokenLookup2 == NULL) {
        return finalize->result;
    }

    if (g_hdr.flags & ETH_PLUGIN_HDR_REGISTER_ASSET) {
        // The plugin points the lookup at an address inside its context, so
        // registering that exact address is what makes the resolve succeed.
        if (finalize->tokenLookup1 != NULL) {
            fuzz_register_token(finalize->tokenLookup1, &g_hdr);
        }
        if (finalize->tokenLookup2 != NULL) {
            fuzz_register_token(finalize->tokenLookup2, &g_hdr);
        }
    }

    eth_plugin_prepare_provide_info(&info);
    info.item1 = get_matching_asset_info(&chain_id, finalize->tokenLookup1);
    info.item2 = get_matching_asset_info(&chain_id, finalize->tokenLookup2);
    if (eth_plugin_call(ETH_PLUGIN_PROVIDE_INFO, &info) <= ETH_PLUGIN_RESULT_UNSUCCESSFUL) {
        return ETH_PLUGIN_RESULT_ERROR;
    }
    *additional_screens = info.additionalScreens;
    return info.result;
}

/** Contract-ID then one query per screen, under the production item cap. */
static void query_ui(uint8_t num_screens, uint8_t additional_screens) {
    size_t total = (size_t) num_screens + (size_t) additional_screens;
    char title[32];
    char msg[80];

    if (total > MAX_PLUGIN_UI_ITEMS || !plugin_ui_get_id()) {
        return;
    }
    for (size_t i = 0; i < total; i++) {
        dataContext.tokenContext.pluginUiCurrentItem = (uint8_t) i;
        if (!plugin_ui_get_item_internal((uint8_t *) title,
                                         sizeof(title),
                                         (uint8_t *) msg,
                                         sizeof(msg))) {
            return;
        }
    }
}

/** Table entry @p index, or the header's raw bytes when the row has no table. */
static void pick_or_draw(uint8_t *out,
                         uint8_t len,
                         const uint8_t *const *table,
                         uint8_t count,
                         uint8_t index,
                         const uint8_t *raw) {
    if (table == NULL || count == 0) {
        memcpy(out, raw, len);
        return;
    }
    memcpy(out, table[index % count], len);
}

void fuzz_app_dispatch(void *cmd_v) {
    const command_t *cmd = (const command_t *) cmd_v;
    const eth_fuzz_plugin_t *plugin = &k_plugins[cmd->ins % ARRAYLEN(k_plugins)];
    uint8_t address[ADDRESS_LENGTH];
    uint8_t selector[SELECTOR_SIZE];
    ethPluginInitContract_t init;
    ethPluginFinalize_t finalize;
    uint8_t additional_screens = 0;

    fuzz_build_tx_content(&tmpContent.txContent, &g_hdr);

    pick_or_draw(address,
                 sizeof(address),
                 plugin->addresses,
                 plugin->num_addresses,
                 cmd->p1,
                 g_hdr.dest);
    pick_or_draw(selector,
                 sizeof(selector),
                 plugin->selectors,
                 plugin->num_selectors,
                 cmd->p1,
                 g_hdr.selector);

    // eth_plugin_perform_init_default() compares the caller's address and
    // selector against these, so the preset plugins need them to agree.
    pluginType = plugin->type;
    memcpy(dataContext.tokenContext.contractAddress, address, sizeof(address));
    memcpy(dataContext.tokenContext.methodSelector, selector, sizeof(selector));
    dataContext.tokenContext.pluginChainId = PLUGIN_CHAIN_ID_ANY;

    switch (plugin->type) {
        case PLUGIN_TYPE_ERC721:
        case PLUGIN_TYPE_ERC1155:
            // Production reaches these only after PROVIDE_NFT_INFORMATION.
            fuzz_register_nft(address, &g_hdr);
            break;
        case PLUGIN_TYPE_SWAP_WITH_CALLDATA:
            // Exchange owns this buffer on device and the invariant keeps the
            // pointer NULL, so the harness supplies the storage.
            G_swap_crosschain_hash = g_hdr.crosschain_hash;
            G_called_from_swap = true;
            G_swap_mode = (swap_mode_t) (g_hdr.swap_mode % (SWAP_MODE_ERROR + 1));
            break;
        default:
            break;
    }

    eth_plugin_prepare_init(&init, selector, (uint32_t) (SELECTOR_SIZE + fuzz_tail_len));
    if (eth_plugin_perform_init(address, &init) <= ETH_PLUGIN_RESULT_UNSUCCESSFUL) {
        return;
    }

    if (!provide_parameters(fuzz_tail_ptr, fuzz_tail_len)) {
        return;
    }

    eth_plugin_prepare_finalize(&finalize);
    if (eth_plugin_call(ETH_PLUGIN_FINALIZE, &finalize) <= ETH_PLUGIN_RESULT_UNSUCCESSFUL) {
        return;
    }

    finalize.result = provide_info(&finalize, &additional_screens);
    // numScreens shares a union with an amount pointer, so it only means a screen
    // count under ETH_UI_TYPE_GENERIC -- the gate finalize_parsing_helper() applies.
    if (finalize.result != ETH_PLUGIN_RESULT_FALLBACK && finalize.uiType == ETH_UI_TYPE_GENERIC) {
        query_ui(finalize.numScreens, additional_screens);
    }
}

int fuzz_entry(const uint8_t *data, size_t size) {
    memset(&g_hdr, 0, sizeof(g_hdr));
    if (size >= (size_t) FUZZ_CTRL_LEN + sizeof(g_hdr)) {
        memcpy(&g_hdr, data + FUZZ_CTRL_LEN, sizeof(g_hdr));
    }
    return fuzz_harness_entry(data, size);
}
