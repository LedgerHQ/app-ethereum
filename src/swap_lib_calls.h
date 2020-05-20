#ifndef SWAP_LIB_CALLS
#define SWAP_LIB_CALLS

#define SIGN_TRANSACTION_IN 0x2000
#define SIGN_TRANSACTION_OUT 0x2001

#define CHECK_ADDRESS_IN 0x3000
#define CHECK_ADDRESS_OUT 0x3001

#define GET_PRINTABLE_AMOUNT_IN 0x4000
#define GET_PRINTABLE_AMOUNT_OUT 0x4001

// structure that should be send to specific coin application to get address
typedef struct check_address_parameters_s {
    // IN
    unsigned char* coin_configuration;
    unsigned char coin_configuration_length;
    // serialized path, segwit, version prefix, hash used, dictionary etc. 
    // fields and serialization format depends on spesific coin app
    unsigned char* address_parameters; 
    unsigned char address_parameters_length;
    char *address_to_check;
    char *extra_id_to_check;
    // OUT
    int result;
} check_address_parameters_t;

// structure that should be send to specific coin application to get printable amount
typedef struct get_printable_amount_parameters_s {
    // IN
    unsigned char* coin_configuration;
    unsigned char coin_configuration_length;
    unsigned char* amount; 
    unsigned char amount_length;
    // OUT
    char printable_amount[30];
} get_printable_amount_parameters_t;

typedef struct create_transaction_parameters_s {
    unsigned char* coin_configuration;
    unsigned char coin_configuration_length;
    unsigned char* amount; 
    unsigned char amount_length;
    unsigned char* fee_amount; 
    unsigned char fee_amount_length;
    char *destination_address;
    char *destination_address_extra_id;
} create_transaction_parameters_t;

#endif
