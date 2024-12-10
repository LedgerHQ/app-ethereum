#include <string.h>
#include "os_utils.h"
#include "os_pic.h"
#include "network.h"
#include "network_dynamic.h"
#include "shared_context.h"
#include "common_utils.h"

const char g_unknown_ticker[] = "???";

// Mapping of chain ids to networks.
static const network_info_t NETWORK_MAPPING[] = {
    {.chain_id = 1, .name = "Ethereum", .ticker = "ETH"},
    {.chain_id = 3, .name = "Ropsten", .ticker = "ETH"},
    {.chain_id = 4, .name = "Rinkeby", .ticker = "ETH"},
    {.chain_id = 5, .name = "Goerli", .ticker = "ETH"},
    {.chain_id = 10, .name = "OP Mainnet", .ticker = "ETH"},
    {.chain_id = 14, .name = "Flare", .ticker = "FLR"},
    {.chain_id = 16, .name = "Flare Coston", .ticker = "FLR"},
    {.chain_id = 19, .name = "Songbird", .ticker = "SGB"},
    {.chain_id = 24, .name = "KardiaChain", .ticker = "KAI"},
    {.chain_id = 25, .name = "Cronos", .ticker = "CRO"},
    {.chain_id = 30, .name = "Rootstock", .ticker = "RBTC"},
    {.chain_id = 40, .name = "Telos EVM Mainnet", .ticker = "TLOS"},
    {.chain_id = 42, .name = "LUKSO", .ticker = "LYX"},
    {.chain_id = 50, .name = "XDC", .ticker = "XDC"},
    {.chain_id = 51, .name = "Apothemnetwork", .ticker = "TXDC"},
    {.chain_id = 56, .name = "BSC", .ticker = "BNB"},
    {.chain_id = 57, .name = "Syscoin", .ticker = "SYS"},
    {.chain_id = 61, .name = "Ethereum Classic", .ticker = "ETC"},
    {.chain_id = 66, .name = "OKXChain", .ticker = "OKT"},
    {.chain_id = 82, .name = "Meter", .ticker = "MTR"},
    {.chain_id = 99, .name = "POA", .ticker = "POA"},
    {.chain_id = 100, .name = "Gnosis", .ticker = "xDAI"},
    {.chain_id = 106, .name = "Velas EVM", .ticker = "VLX"},
    {.chain_id = 137, .name = "Polygon", .ticker = "POL"},
    {.chain_id = 138, .name = "Defi Oracle Meta", .ticker = "ETH"},
    {.chain_id = 196, .name = "OKBChain Mainnet", .ticker = "OKB"},
    {.chain_id = 199, .name = "BTTC", .ticker = "BTT"},
    {.chain_id = 246, .name = "EnergyWebChain", .ticker = "EWT"},
    {.chain_id = 248, .name = "Oasys", .ticker = "OAS"},
    {.chain_id = 250, .name = "Fantom", .ticker = "FTM"},
    {.chain_id = 288, .name = "Boba Network", .ticker = "ETH"},
    {.chain_id = 300, .name = "ZKsync Sepolia Testnet", .ticker = "ETH"},
    {.chain_id = 321, .name = "KCC", .ticker = "KCS"},
    {.chain_id = 324, .name = "ZKsync Era", .ticker = "ETH"},
    {.chain_id = 336, .name = "Shiden", .ticker = "SDN"},
    {.chain_id = 369, .name = "PulseChain", .ticker = "PLS"},
    {.chain_id = 592, .name = "Astar", .ticker = "ASTR"},
    {.chain_id = 888, .name = "Wanchain", .ticker = "WAN"},
    {.chain_id = 943, .name = "PulseChain Testnet", .ticker = "tPLS"},
    {.chain_id = 1030, .name = "Conflux", .ticker = "CFX"},
    {.chain_id = 1088, .name = "Metis Andromeda", .ticker = "METIS"},
    {.chain_id = 1101, .name = "Polygon zkEVM", .ticker = "ETH"},
    {.chain_id = 1116, .name = "Core", .ticker = "CORE"},
    {.chain_id = 1135, .name = "Lisk", .ticker = "ETH"},
    {.chain_id = 1284, .name = "Moonbeam", .ticker = "GLMR"},
    {.chain_id = 1285, .name = "Moonriver", .ticker = "MOVR"},
    {.chain_id = 1818, .name = "Cube", .ticker = "CUBE"},
    {.chain_id = 1907, .name = "Bitcichain", .ticker = "BITCI"},
    {.chain_id = 2222, .name = "Kava EVM", .ticker = "KAVA"},
    {.chain_id = 3776, .name = "Astar zkEVM", .ticker = "ETH"},
    {.chain_id = 4201, .name = "LUKSO Testnet", .ticker = "LYXt"},
    {.chain_id = 4202, .name = "Lisk Sepolia Testnet", .ticker = "ETH"},
    {.chain_id = 4689, .name = "IoTeX", .ticker = "IOTX"},
    {.chain_id = 4690, .name = "IoTeX Testnet", .ticker = "IOTX"},
    {.chain_id = 4919, .name = "Venidium", .ticker = "XVM"},
    {.chain_id = 5000, .name = "Mantle", .ticker = "MNT"},
    {.chain_id = 5003, .name = "Mantle Sepolia", .ticker = "MNT"},
    {.chain_id = 7000, .name = "ZetaChain", .ticker = "ZETA"},
    {.chain_id = 7171, .name = "Bitrock Mainnet", .ticker = "BROCK"},
    {.chain_id = 7341, .name = "Shyft", .ticker = "SHFT"},
    {.chain_id = 8217, .name = "Klaytn Cypress", .ticker = "KLAY"},
    {.chain_id = 8453, .name = "Base", .ticker = "ETH"},
    {.chain_id = 9001, .name = "Evmos", .ticker = "EVMOS"},
    {.chain_id = 10200, .name = "Chiado", .ticker = "xDAI"},
    {.chain_id = 10507, .name = "Numbers Protocol", .ticker = "NUM"},
    {.chain_id = 17000, .name = "Holesky", .ticker = "ETH"},
    {.chain_id = 39797, .name = "Energi", .ticker = "NRG"},
    {.chain_id = 42161, .name = "Arbitrum", .ticker = "ETH"},
    {.chain_id = 42220, .name = "Celo", .ticker = "CELO"},
    {.chain_id = 42793, .name = "Etherlink Mainnet", .ticker = "XTZ"},
    {.chain_id = 43114, .name = "Avalanche", .ticker = "AVAX"},
    {.chain_id = 44787, .name = "Celo Alfajores", .ticker = "aCELO"},
    {.chain_id = 47763, .name = "Neo X Mainnet", .ticker = "GAS"},
    {.chain_id = 52014, .name = "Electroneum", .ticker = "ETN"},
    {.chain_id = 59141, .name = "Linea Sepolia", .ticker = "ETH"},
    {.chain_id = 59144, .name = "Linea", .ticker = "ETH"},
    {.chain_id = 60808, .name = "BOB", .ticker = "ETH"},
    {.chain_id = 62320, .name = "Celo Baklava", .ticker = "bCELO"},
    {.chain_id = 62621, .name = "Multivac", .ticker = "MTV"},
    {.chain_id = 73799, .name = "Volta", .ticker = "VOLTA"},
    {.chain_id = 81457, .name = "Blast", .ticker = "ETH"},
    {.chain_id = 84532, .name = "Base Sepolia", .ticker = "ETH"},
    {.chain_id = 200810, .name = "Bitlayer Testnet", .ticker = "BTC"},
    {.chain_id = 200901, .name = "Bitlayer", .ticker = "BTC"},
    {.chain_id = 421614, .name = "Arbitrum Sepolia", .ticker = "ETH"},
    {.chain_id = 534351, .name = "Scroll Sepolia", .ticker = "ETH"},
    {.chain_id = 534352, .name = "Scroll", .ticker = "ETH"},
    {.chain_id = 534353, .name = "Scroll Alpha", .ticker = "ETH"},
    {.chain_id = 543210, .name = "Zero", .ticker = "ETH"},
    {.chain_id = 5201420, .name = "Electroneum Testnet", .ticker = "ETN"},
    {.chain_id = 11155111, .name = "Sepolia", .ticker = "ETH"},
    {.chain_id = 11155420, .name = "OP Sepolia", .ticker = "ETH"},
    {.chain_id = 12227332, .name = "Neo X Testnet", .ticker = "GAS"},
    {.chain_id = 20531811, .name = "TecraTestnet", .ticker = "TCR"},
    {.chain_id = 20531812, .name = "Tecra", .ticker = "TCR"},
    {.chain_id = 168587773, .name = "Blast Sepolia", .ticker = "ETH"},
    {.chain_id = 245022926, .name = "Neon EVM Devnet", .ticker = "NEON"},
    {.chain_id = 245022934, .name = "Neon EVM Mainnet", .ticker = "NEON"},
    {.chain_id = 11297108109, .name = "Palm Network", .ticker = "PALM"},
};

static const network_info_t *get_network_from_chain_id(const uint64_t *chain_id) {
    if (*chain_id != 0) {
        // Look if the network is available
        for (size_t i = 0; i < MAX_DYNAMIC_NETWORKS; i++) {
            if (DYNAMIC_NETWORK_INFO[i].chain_id == *chain_id) {
                PRINTF("[NETWORK] - Found dynamic %s\n", DYNAMIC_NETWORK_INFO[i].name);
                return (const network_info_t *) &DYNAMIC_NETWORK_INFO[i];
            }
        }
        // Fallback to hardcoded table
        for (size_t i = 0; i < ARRAYLEN(NETWORK_MAPPING); i++) {
            if (NETWORK_MAPPING[i].chain_id == *chain_id) {
                PRINTF("[NETWORK] - Fallback on hardcoded list. Found %s\n",
                       NETWORK_MAPPING[i].name);
                return (const network_info_t *) &NETWORK_MAPPING[i];
            }
        }
    }
    return NULL;
}

const char *get_network_name_from_chain_id(const uint64_t *chain_id) {
    const network_info_t *net = get_network_from_chain_id(chain_id);

    if (net == NULL) {
        return NULL;
    }
    return PIC(net->name);
}

const char *get_network_ticker_from_chain_id(const uint64_t *chain_id) {
    const network_info_t *net = get_network_from_chain_id(chain_id);

    if (net == NULL) {
        return NULL;
    }
    return PIC(net->ticker);
}

bool chain_is_ethereum_compatible(const uint64_t *chain_id) {
    return get_network_from_chain_id(chain_id) != NULL;
}

// Returns the chain ID. Defaults to 0 if txType was not found (For TX).
uint64_t get_tx_chain_id(void) {
    uint64_t chain_id = 0;

    switch (txContext.txType) {
        case LEGACY:
            chain_id = u64_from_BE(txContext.content->v, txContext.content->vLength);
            break;
        case EIP2930:
        case EIP1559:
            chain_id = u64_from_BE(tmpContent.txContent.chainID.value,
                                   tmpContent.txContent.chainID.length);
            break;
        default:
            PRINTF("Txtype `%d` not supported while generating chainID\n", txContext.txType);
            break;
    }
    return chain_id;
}

const char *get_displayable_ticker(const uint64_t *chain_id, const chain_config_t *chain_cfg) {
    const char *ticker = get_network_ticker_from_chain_id(chain_id);

    if (ticker == NULL) {
        if (*chain_id == chain_cfg->chainId) {
            ticker = chain_cfg->coinName;
        } else {
            ticker = g_unknown_ticker;
        }
    }
    return ticker;
}

/**
 * Checks whether the app can support the given chain ID
 *
 * - If the given chain ID is the same as the app's one
 * - If both chain IDs are present in the array of Ethereum-compatible networks
 */
bool app_compatible_with_chain_id(const uint64_t *chain_id) {
    return ((chainConfig->chainId == *chain_id) ||
            (chain_is_ethereum_compatible(&chainConfig->chainId) &&
             chain_is_ethereum_compatible(chain_id)));
}
