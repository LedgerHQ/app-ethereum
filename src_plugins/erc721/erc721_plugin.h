#include <string.h>
#include "eth_plugin_internal.h"
#include "eth_plugin_handler.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "utils.h"

#define APPROVAL_DATA_SIZE       SELECTOR_SIZE + 2 * PARAMETER_LENGTH
#define MAX_COLLECTION_NAME_SIZE 40

typedef enum {
    APPROVAL,
    TRANSFER,
} erc721_selector_t;

typedef enum {
    FIRST,
} erc721_selector_field;

typedef struct erc721_parameters_t {
    uint8_t selectorIndex;
    uint8_t address[ADDRESS_LENGTH];
    uint8_t tokenId[INT256_LENGTH];
    char collection_name[MAX_COLLECTION_NAME_SIZE];

    erc721_selector_field next_param;
} erc721_parameters_t;

void handle_contract_ui(void *parameters);
void handle_provide_parameter(void *parameters) {