#pragma once

/* This file is the shared API between Exchange and the apps started in Library mode for Exchange
 *
 * DO NOT MODIFY THIS FILE IN APPLICATIONS OTHER THAN EXCHANGE
 * On modification in Exchange, forward the changes to all applications supporting Exchange
 */

#include <stdbool.h>
#include <stdint.h>
#include "swap_lib_calls.h"
#include "chainConfig.h"
#include "shared_context.h"
#include "caller_api.h"

typedef struct eth_libargs_s {
    unsigned int id;
    unsigned int command;
    chain_config_t *chain_config;
    union {
        check_address_parameters_t *check_address;
        create_transaction_parameters_t *create_transaction;
        get_printable_amount_parameters_t *get_printable_amount;
        caller_app_t *caller_app;
    };
} eth_libargs_t;
