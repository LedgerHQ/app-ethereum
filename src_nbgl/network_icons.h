#ifndef NETWORK_ICONS_H_
#define NETWORK_ICONS_H_

#include <stdbool.h>
#include "nbgl_types.h"

const nbgl_icon_details_t *get_network_icon_from_chain_id(const uint64_t *chain_id);

#endif  // NETWORK_ICONS_H_
