#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>
#include <stdbool.h>

const char *get_network_name_from_chain_id(const uint64_t *chain_id);
const char *get_network_ticker_from_chain_id(const uint64_t *chain_id);

bool chain_is_ethereum_compatible(const uint64_t *chain_id);

#endif  // _NETWORK_H_
