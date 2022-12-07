#ifndef _SWAP_LIB_CALLS_H_
#define _SWAP_LIB_CALLS_H_

#include "stdbool.h"
#include "chainConfig.h"

#define RUN_APPLICATION 1

#define SIGN_TRANSACTION 2

#define CHECK_ADDRESS 3

#define GET_PRINTABLE_AMOUNT 4

#define MAX_PRINTABLE_AMOUNT_SIZE 50

// structure that should be send to specific coin application to get address
typedef struct check_address_parameters_s {
    // IN
    const unsigned char* const coin_configuration;
    const unsigned char coin_configuration_length;
    // serialized path, segwit, version prefix, hash used, dictionary etc.
    // fields and serialization format depends on spesific coin app
    const unsigned char* const address_parameters;
    const unsigned char address_parameters_length;
    const char* const address_to_check;
    const char* const extra_id_to_check;
    // OUT
    int result;
} check_address_parameters_t;

// structure that should be send to specific coin application to get printable amount
typedef struct get_printable_amount_parameters_s {
    // IN
    const unsigned char* const coin_configuration;
    const unsigned char coin_configuration_length;
    const unsigned char* const amount;
    const unsigned char amount_length;
    const bool is_fee;
    // OUT
    char printable_amount[MAX_PRINTABLE_AMOUNT_SIZE];
    // int result;
} get_printable_amount_parameters_t;

typedef struct create_transaction_parameters_s {
    const unsigned char* const coin_configuration;
    const unsigned char coin_configuration_length;
    const unsigned char* const amount;
    const unsigned char amount_length;
    const unsigned char* const fee_amount;
    const unsigned char fee_amount_length;
    const char* const destination_address;
    const char* const destination_address_extra_id;
} create_transaction_parameters_t;

typedef struct libargs_s {
    unsigned int id;
    unsigned int command;
    chain_config_t *chain_config;
    union {
        check_address_parameters_t *check_address;
        create_transaction_parameters_t *create_transaction;
        get_printable_amount_parameters_t *get_printable_amount;
        char *plugin_name;
    };
} libargs_t;

#endif  // _SWAP_LIB_CALLS_H_
