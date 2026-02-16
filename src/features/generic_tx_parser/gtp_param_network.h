/**
 * @file gtp_param_network.h
 * @brief Generic transaction parser - Network parameter definitions
 *
 * This header defines the structures and functions for handling network parameters
 * in the generic transaction parser. Network parameters represent blockchain network
 * identifiers (chain IDs) that are validated and displayed to the user.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "common_utils.h"
#include "gtp_value.h"
#include "tlv.h"

/**
 * @brief Network parameter structure
 *
 * Contains the network information including version and chain ID value.
 */
typedef struct {
    uint8_t version; /**< Version of the network parameter structure */
    s_value value;   /**< Chain ID value */
} s_param_network;

/**
 * @brief Network parameter parsing context
 *
 * Context structure used during TLV parsing of network parameters.
 */
typedef struct {
    s_param_network *param; /**< Pointer to the network parameter being parsed */
} s_param_network_context;

bool handle_param_network_struct(const s_tlv_data *data, s_param_network_context *context);
bool format_param_network(const s_param_network *param, const char *name);
