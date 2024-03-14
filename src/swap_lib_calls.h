#pragma once

/* This file is the shared API between Exchange and the apps started in Library mode for Exchange
 *
 * DO NOT MODIFY THIS FILE IN APPLICATIONS OTHER THAN EXCHANGE
 * On modification in Exchange, forward the changes to all applications supporting Exchange
 */

#include "stdbool.h"
#include "chainConfig.h"
#include "shared_context.h"
#include "stdint.h"
#include "caller_api.h"

#define RUN_APPLICATION 1

#define SIGN_TRANSACTION 2

#define CHECK_ADDRESS 3

#define GET_PRINTABLE_AMOUNT 4

/*
 * Amounts are stored as bytes, with a max size of 16 (see protobuf
 * specifications). Max 16B integer is 340282366920938463463374607431768211455
 * in decimal, which is a 32-long char string.
 * The printable amount also contains spaces, the ticker symbol (with variable
 * size, up to 12 in Ethereum for instance) and a terminating null byte, so 50
 * bytes total should be a fair maximum.
 */
#define MAX_PRINTABLE_AMOUNT_SIZE 50

// structure that should be send to specific coin application to get address
typedef struct check_address_parameters_s {
    // IN
    uint8_t *coin_configuration;
    uint8_t coin_configuration_length;
    // serialized path, segwit, version prefix, hash used, dictionary etc.
    // fields and serialization format depends on specific coin app
    uint8_t *address_parameters;
    uint8_t address_parameters_length;
    char *address_to_check;
    char *extra_id_to_check;
    // OUT
    int result;
} check_address_parameters_t;

// structure that should be send to specific coin application to get printable amount
typedef struct get_printable_amount_parameters_s {
    // IN
    uint8_t *coin_configuration;
    uint8_t coin_configuration_length;
    uint8_t *amount;
    uint8_t amount_length;
    bool is_fee;
    // OUT
    char printable_amount[MAX_PRINTABLE_AMOUNT_SIZE];
} get_printable_amount_parameters_t;

typedef struct create_transaction_parameters_s {
    // IN
    uint8_t *coin_configuration;
    uint8_t coin_configuration_length;
    uint8_t *amount;
    uint8_t amount_length;
    uint8_t *fee_amount;
    uint8_t fee_amount_length;
    char *destination_address;
    char *destination_address_extra_id;
    // OUT
    uint8_t result;
} create_transaction_parameters_t;

typedef struct libargs_s {
    unsigned int id;
    unsigned int command;
    chain_config_t *chain_config;
    union {
        check_address_parameters_t *check_address;
        create_transaction_parameters_t *create_transaction;
        get_printable_amount_parameters_t *get_printable_amount;
        caller_app_t *caller_app;
    };
} libargs_t;
