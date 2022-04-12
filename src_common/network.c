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
    {.chain_id = 11297108109, .name = "Palm Network", .ticker = "PALM "}};

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
