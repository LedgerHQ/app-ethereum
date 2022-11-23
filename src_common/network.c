#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "network.h"
#include "os.h"
#include "shared_context.h"
#include "utils.h"

// Mappping of chain ids to networks.
const network_info_t NETWORK_MAPPING[] = {
    {.chain_id = 1, .name = "Ethereum", .ticker = "ETH "},
    {.chain_id = 3, .name = "Ropsten", .ticker = "ETH "},
    {.chain_id = 4, .name = "Rinkeby", .ticker = "ETH "},
    {.chain_id = 5, .name = "Goerli", .ticker = "ETH "},
    {.chain_id = 10, .name = "Optimism", .ticker = "ETH "},
    {.chain_id = 42, .name = "Kovan", .ticker = "ETH "},
    {.chain_id = 56, .name = "BSC", .ticker = "BNB "},
    {.chain_id = 100, .name = "xDai", .ticker = "xDAI "},
    {.chain_id = 137, .name = "Polygon", .ticker = "MATIC "},
    {.chain_id = 250, .name = "Fantom", .ticker = "FTM "},
    {.chain_id = 42161, .name = "Arbitrum", .ticker = "AETH "},
    {.chain_id = 42220, .name = "Celo", .ticker = "CELO "},
    {.chain_id = 43114, .name = "Avalanche", .ticker = "AVAX "},
    {.chain_id = 44787, .name = "Celo Alfajores", .ticker = "aCELO "},
    {.chain_id = 62320, .name = "Celo Baklava", .ticker = "bCELO "},
    {.chain_id = 11297108109, .name = "Palm Network", .ticker = "PALM "},
    {.chain_id = 1818, .name = "Cube", .ticker = "CUBE "},
    {.chain_id = 336, .name = "Shiden", .ticker = "SDN "},
    {.chain_id = 592, .name = "Astar", .ticker = "ASTR "},
    {.chain_id = 50, .name = "XDC", .ticker = "XDC "},
    {.chain_id = 82, .name = "Meter", .ticker = "MTR "},
    {.chain_id = 62621, .name = "Multivac", .ticker = "MTV "},
    {.chain_id = 20531812, .name = "Tecra", .ticker = "TCR "},
    {.chain_id = 20531811, .name = "TecraTestnet", .ticker = "TCR "},
    {.chain_id = 51, .name = "Apothemnetwork", .ticker = "XDC "},
    {.chain_id = 199, .name = "BTTC", .ticker = "BTT "},
    {.chain_id = 1030, .name = "Conflux", .ticker = "CFX "},
    {.chain_id = 61, .name = "Ethereum Classic", .ticker = "ETC "},
    {.chain_id = 246, .name = "EnergyWebChain", .ticker = "EWC "},
    {.chain_id = 14, .name = "Flare", .ticker = "FLR "},
    {.chain_id = 16, .name = "Flare Coston", .ticker = "FLR "},
    {.chain_id = 24, .name = "KardiaChain", .ticker = "KAI "},
    {.chain_id = 1284, .name = "Moonbeam", .ticker = "GLMR "},
    {.chain_id = 1285, .name = "Moonriver", .ticker = "MOVR "},
    {.chain_id = 66, .name = "OKXChain", .ticker = "OKT "},
    {.chain_id = 99, .name = "POA", .ticker = "POA "},
    {.chain_id = 7341, .name = "Shyft", .ticker = "SHFT "},
    {.chain_id = 19, .name = "Songbird", .ticker = "SGB "},
    {.chain_id = 73799, .name = "Volta", .ticker = "VOLTA "},
    {.chain_id = 25, .name = "Cronos", .ticker = "CRO "},
    {.chain_id = 534354, .name = "Scroll (Pre-Alpha)", .ticker = "SCR "},
    {.chain_id = 534353, .name = "Scroll (Goerli)", .ticker = "SCR "},
    {.chain_id = 534352, .name = "Scroll", .ticker = "SCR "}};
    {.chain_id = 8217, .name = "Klaytn", .ticker = "KLAY "}};

uint64_t get_chain_id(void) {
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

const network_info_t *get_network(void) {
    uint64_t chain_id = get_chain_id();
    for (size_t i = 0; i < sizeof(NETWORK_MAPPING) / sizeof(*NETWORK_MAPPING); i++) {
        if (NETWORK_MAPPING[i].chain_id == chain_id) {
            return (const network_info_t *) PIC(&NETWORK_MAPPING[i]);
        }
    }
    return NULL;
}

const char *get_network_name(void) {
    const network_info_t *network = get_network();
    if (network == NULL) {
        return NULL;
    } else {
        return (const char *) PIC(network->name);
    }
}

const char *get_network_ticker(void) {
    const network_info_t *network = get_network();
    if (network == NULL) {
        return chainConfig->coinName;
    } else {
        return (char *) PIC(network->ticker);
    }
}
