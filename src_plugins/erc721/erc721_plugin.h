#include <string.h>
#include "eth_plugin_internal.h"
#include "eth_plugin_handler.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "utils.h"

// Internal plugin for EIP 721: https://eips.ethereum.org/EIPS/eip-721

#define MAX_COLLECTION_NAME_SIZE 40

typedef enum {
    APPROVE,
    SET_APPROVAL_FOR_ALL,
    TRANSFER,
    SAFE_TRANSFER,
    SAFE_TRANSFER_DATA,
} erc721_selector_t;

typedef enum {
    FROM,
    TO,
    DATA,
    TOKEN_ID,
    OPERATOR,
    NONE,
} erc721_selector_field;

typedef struct erc721_parameters_t {
    uint8_t selectorIndex;
    uint8_t address[ADDRESS_LENGTH];
    uint8_t tokenId[INT256_LENGTH];
    char collection_name[MAX_COLLECTION_NAME_SIZE];

    erc721_selector_field next_param;
} erc721_parameters_t;

void handle_provide_parameter(void *parameters);
void handle_query_contract_ui(void *parameters);
bool erc721_plugin_available_check();