#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "chain_id.h"
#include "os.h"


// Sizes of name should not be bigger than NETWORK_NAME_SIZE_MAX;
const network_info_t NETWORK_MAPPING[] = {
    {.chain_id = 1, .name = "Ethereum"},
    {.chain_id = 3, .name = "Ropsten"},
    {.chain_id = 4, .name = "Rinkeby"},
    {.chain_id = 5, .name = "Goerli"},
    {.chain_id = 10, .name = "Optimism"},
    {.chain_id = 42, .name = "Kovan"},
    {.chain_id = 56, .name = "BSC"},
    {.chain_id = 100, .name = "xDai"},
    {.chain_id = 137, .name = "Polygon"},
    {.chain_id = 43114, .name = "Avalanche"}
};


// Returns a pointer to the name represented as a string, or NULL if the `chain_id` if there is none.
static char *get_name_from_chain_id(uint32_t chain_id) {
    for (uint8_t i = 0; i < sizeof(NETWORK_MAPPING) / sizeof(*NETWORK_MAPPING); i++) {
        if (NETWORK_MAPPING[i].chain_id == chain_id) {
            return (char *) PIC(NETWORK_MAPPING[i].name);
        }
    }
    return NULL;
}


// Fills `dest` with the network name corresponding to `chain_id`.
// Returns a pointer to `dest` if successful, else returns `NULL`.
char *get_network_name_from_chain_id(char *dest, uint8_t dest_size, uint32_t chain_id) {
    if (dest_size == 0 || dest == NULL) {
        return NULL;
    }

    char *network_name = get_name_from_chain_id(chain_id);

    if (network_name == NULL) {
        return NULL;
    }

    strncpy(dest, network_name, dest_size);

    // Make sure we null terminate the string.
    dest[dest_size - 1] = 0;
    return dest;
}