#include <string.h>
#include "os_utils.h"
#include "os_pic.h"
#include "network.h"

typedef struct network_info_s {
    const char *name;
    const char *ticker;
    uint64_t chain_id;
} network_info_t;

// Mappping of chain ids to networks.
static const network_info_t NETWORK_MAPPING[] = {
    {.chain_id = 1, .name = "Ethereum", .ticker = "ETH"},
    {.chain_id = 3, .name = "Ropsten", .ticker = "ETH"},
    {.chain_id = 4, .name = "Rinkeby", .ticker = "ETH"},
    {.chain_id = 5, .name = "Goerli", .ticker = "ETH"},
    {.chain_id = 10, .name = "Optimism", .ticker = "ETH"},
    {.chain_id = 42, .name = "Kovan", .ticker = "ETH"},
    {.chain_id = 56, .name = "BSC", .ticker = "BNB"},
    {.chain_id = 100, .name = "xDai", .ticker = "xDAI"},
    {.chain_id = 137, .name = "Polygon", .ticker = "MATIC"},
    {.chain_id = 250, .name = "Fantom", .ticker = "FTM"},
    {.chain_id = 42161, .name = "Arbitrum", .ticker = "ETH"},
    {.chain_id = 42220, .name = "Celo", .ticker = "CELO"},
    {.chain_id = 43114, .name = "Avalanche", .ticker = "AVAX"},
    {.chain_id = 44787, .name = "Celo Alfajores", .ticker = "aCELO"},
    {.chain_id = 62320, .name = "Celo Baklava", .ticker = "bCELO"},
    {.chain_id = 11297108109, .name = "Palm Network", .ticker = "PALM"},
    {.chain_id = 1818, .name = "Cube", .ticker = "CUBE"},
    {.chain_id = 336, .name = "Shiden", .ticker = "SDN"},
    {.chain_id = 592, .name = "Astar", .ticker = "ASTR"},
    {.chain_id = 50, .name = "XDC", .ticker = "XDC"},
    {.chain_id = 82, .name = "Meter", .ticker = "MTR"},
    {.chain_id = 62621, .name = "Multivac", .ticker = "MTV"},
    {.chain_id = 20531812, .name = "Tecra", .ticker = "TCR"},
    {.chain_id = 20531811, .name = "TecraTestnet", .ticker = "TCR"},
    {.chain_id = 51, .name = "Apothemnetwork", .ticker = "XDC"},
    {.chain_id = 199, .name = "BTTC", .ticker = "BTT"},
    {.chain_id = 1030, .name = "Conflux", .ticker = "CFX"},
    {.chain_id = 61, .name = "Ethereum Classic", .ticker = "ETC"},
    {.chain_id = 246, .name = "EnergyWebChain", .ticker = "EWT"},
    {.chain_id = 14, .name = "Flare", .ticker = "FLR"},
    {.chain_id = 16, .name = "Flare Coston", .ticker = "FLR"},
    {.chain_id = 24, .name = "KardiaChain", .ticker = "KAI"},
    {.chain_id = 1284, .name = "Moonbeam", .ticker = "GLMR"},
    {.chain_id = 1285, .name = "Moonriver", .ticker = "MOVR"},
    {.chain_id = 66, .name = "OKXChain", .ticker = "OKT"},
    {.chain_id = 99, .name = "POA", .ticker = "POA"},
    {.chain_id = 7341, .name = "Shyft", .ticker = "SHFT"},
    {.chain_id = 19, .name = "Songbird", .ticker = "SGB"},
    {.chain_id = 73799, .name = "Volta", .ticker = "VOLTA"},
    {.chain_id = 25, .name = "Cronos", .ticker = "CRO"},
    {.chain_id = 534354, .name = "Scroll (Pre-Alpha)", .ticker = "SCR"},
    {.chain_id = 534353, .name = "Scroll (Goerli)", .ticker = "SCR"},
    {.chain_id = 534352, .name = "Scroll", .ticker = "SCR"},
    {.chain_id = 321, .name = "KCC", .ticker = "KCS"},
    {.chain_id = 30, .name = "Rootstock", .ticker = "RBTC"},
    {.chain_id = 9001, .name = "Evmos", .ticker = "EVMOS"},
    {.chain_id = 1088, .name = "Metis Andromeda", .ticker = "METIS"},
    {.chain_id = 2222, .name = "Kava EVM", .ticker = "KAVA"},
    {.chain_id = 8217, .name = "Klaytn Cypress", .ticker = "KLAY"},
    {.chain_id = 57, .name = "Syscoin", .ticker = "SYS"},
    {.chain_id = 106, .name = "Velas EVM", .ticker = "VLX"},
    {.chain_id = 288, .name = "Boba Network", .ticker = "ETH"},
    {.chain_id = 39797, .name = "Energi", .ticker = "NRG"},
    {.chain_id = 248, .name = "Oasys", .ticker = "OAS"}};

static const network_info_t *get_network_from_chain_id(const uint64_t *chain_id) {
    for (size_t i = 0; i < ARRAYLEN(NETWORK_MAPPING); i++) {
        if (NETWORK_MAPPING[i].chain_id == *chain_id) {
            return (const network_info_t *) &NETWORK_MAPPING[i];
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
