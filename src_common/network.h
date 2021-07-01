#include <stdint.h>
#include "tokens.h"

#define NETWORK_STRING_MAX_SIZE 12

typedef struct network_info_s {
    const char name[NETWORK_STRING_MAX_SIZE];
    const char ticker[MAX_TICKER_LEN];
    uint32_t chain_id;
} network_info_t;

// Returns the current chain id. Defaults to 0 if txType was not found.
uint32_t get_chain_id(void);
// Returns a pointer to the network struct, or NULL if there is none.
network_info_t *get_network(void);
// Returns a pointer to the network name, or NULL if there is none.
char *get_network_name(void);
// Returns a pointer to the network ticker, or chainConfig->coinName if there is none.
char *get_network_ticker(void);