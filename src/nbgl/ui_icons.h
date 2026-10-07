#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "nbgl_icons.h"
#include "nbgl_types.h"
#include "caller_app.h"

const nbgl_icon_details_t *get_app_icon(bool caller_icon);
const nbgl_icon_details_t *get_home_icon(void);
const nbgl_icon_details_t *get_tx_icon(bool fromPlugin);
const nbgl_icon_details_t *get_network_icon_from_chain_id(const uint64_t *chain_id);
const nbgl_icon_details_t *get_clone_network_icon(const caller_app_t *caller_app);
