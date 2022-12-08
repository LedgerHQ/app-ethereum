#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>
#include "tokens.h"
#include "shared_context.h"

#define MAX_NETWORK_TICKER_LEN 8

typedef struct network_info_s {
    const char *name;
    const char *ticker;
    uint64_t chain_id;
} network_info_t;

// Returns the chain ID. Defaults to 0 if txType was not found (For TX).
uint64_t get_tx_chain_id(void);
uint64_t get_app_chain_id(void);
#ifdef HAVE_NBGL
const nbgl_icon_details_t* get_app_chain_icon(void);
#endif // HAVE_NBGL
// Returns a pointer to the network name, or NULL if there is none.
const char *get_tx_network_name(void);
const char *get_app_network_name(void);

// Returns a pointer to the network ticker, or chainConfig->coinName if there is none.
const char *get_tx_network_ticker(void);
const char *get_app_network_ticker(void);

#endif  // _NETWORK_H_
