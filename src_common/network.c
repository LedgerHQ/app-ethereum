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
    {.chain_id = 534352, .name = "Scroll", .ticker = "SCR "},
    {.chain_id = 321, .name = "KCC", .ticker = "KCS "},
    {.chain_id = 30, .name = "Rootstock", .ticker = "RBTC "},
    {.chain_id = 122, .name = "Fuse", .ticker = "FUSE "},
    {.chain_id = 1666600000, .name = "Harmony shard0", .ticker = "ONE "},
    {.chain_id = 128, .name = "Huobi ECO Chain", .ticker = "HT "},
    {.chain_id = 1313161554, .name = "Aurora", .ticker = "ETH "},
    {.chain_id = 53935, .name = "DFK Chain", .ticker = "JEWEL "},
    {.chain_id = 9001, .name = "Evmos", .ticker = "EVMOS "},
    {.chain_id = 4689, .name = "IoTeX Network", .ticker = "IOTX "},
    {.chain_id = 1088, .name = "Metis Andromeda", .ticker = "METIS "},
    {.chain_id = 40, .name = "Telos EVM", .ticker = "TLOS "},
    {.chain_id = 2222, .name = "Kava EVM", .ticker = "KAVA "},
    {.chain_id = 10000, .name = "Smart Bitcoin Cash", .ticker = "BCH "},
    {.chain_id = 8217, .name = "Klaytn Cypress", .ticker = "KLAY "},
    {.chain_id = 245022934, .name = "Neon EVM", .ticker = "NEON "},
    {.chain_id = 57, .name = "Syscoin", .ticker = "SYS "},
    {.chain_id = 32520, .name = "Bitgert", .ticker = "BRISE "},
    {.chain_id = 106, .name = "Velas EVM", .ticker = "VLX "},
    {.chain_id = 288, .name = "Boba Network", .ticker = "ETH "},
    {.chain_id = 7700, .name = "Canto", .ticker = "CANTO "},
    {.chain_id = 108, .name = "ThunderCore", .ticker = "TT "},
    {.chain_id = 88, .name = "TomoChain", .ticker = "TOMO "},
    {.chain_id = 1024, .name = "CLV Parachain", .ticker = "CLV "},
    {.chain_id = 32659, .name = "Fusion", .ticker = "FSN "},
    {.chain_id = 20, .name = "Elastos Smart Chain", .ticker = "ELA "},
    {.chain_id = 39797, .name = "Energi", .ticker = "NRG "},
    {.chain_id = 820, .name = "Callisto", .ticker = "CLO "},
    {.chain_id = 60, .name = "GoChain", .ticker = "GO "},
    {.chain_id = 2152, .name = "Findora", .ticker = "FRA "},
    {.chain_id = 8, .name = "Ubiq", .ticker = "UBQ "},
    {.chain_id = 269, .name = "High Performance Blockchain", .ticker = "HPB "},
    {.chain_id = 1234, .name = "Step Network", .ticker = "FITFI "},
    {.chain_id = 333999, .name = "Polis Network", .ticker = "POLIS "},
    {.chain_id = 2000, .name = "DogeChain", .ticker = "DOGE "},
    {.chain_id = 416, .name = "SX Network", .ticker = "SX "},
    {.chain_id = 1231, .name = "Ultron", .ticker = "ULX "},
    {.chain_id = 6969, .name = "Tomb Chain", .ticker = "TOMB "},
    {.chain_id = 420420, .name = "KekChain", .ticker = "KEK "},
    {.chain_id = 5551, .name = "Nahmii", .ticker = "ETH "},
    {.chain_id = 55, .name = "Zyx", .ticker = "ZYX "},
    {.chain_id = 877, .name = "Dexit Network", .ticker = "DXT "},
    {.chain_id = 26863, .name = "Oasis Chain", .ticker = "OAC "}};

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
