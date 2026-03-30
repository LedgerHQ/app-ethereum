#include "apdu_constants.h"
#include "public_keys.h"
#include "network.h"
#include "os_pki.h"
#include "app_mem_utils.h"
#include "lists.h"
#include "token_info.h"

typedef struct {
    flist_node_t _list;
    s_token_info info;
} s_token_info_node;

static s_token_info_node *g_token_info_list;

uint16_t handle_provide_erc20_token_information(uint8_t p1,
                                                uint8_t p2,
                                                uint8_t lc,
                                                const uint8_t *data,
                                                unsigned int *tx) {
    uint32_t offset = 0;
    s_token_info_node *node;
    uint8_t ticker_length;
    const char *ticker;
    const uint8_t *addr;
    uint32_t decimals;
    uint32_t chain_id_32;
    uint64_t chain_id;
    uint8_t hash[INT256_LENGTH];
    uint8_t index = 0;

    if ((p1 != 0) || (p2 != 0)) {
        return SWO_INCORRECT_P1_P2;
    }
    if ((offset + sizeof(ticker_length)) > lc) {
        return SWO_INCORRECT_DATA;
    }
    ticker_length = data[offset];
    offset += sizeof(ticker_length);

    if ((offset + ticker_length) > lc) {
        return SWO_INCORRECT_DATA;
    }
    ticker = (const char *) &data[offset];
    offset += ticker_length;

    if ((offset + sizeof(node->info.address)) > lc) {
        return SWO_INCORRECT_DATA;
    }
    addr = &data[offset];
    offset += sizeof(node->info.address);

    // TODO: 4 bytes for this is overkill
    if ((offset + sizeof(decimals)) > lc) {
        return SWO_INCORRECT_DATA;
    }
    decimals = U4BE(data, offset);
    offset += sizeof(decimals);

    // TODO: Handle 64-bit long chain IDs
    if ((offset + sizeof(chain_id_32)) > lc) {
        return SWO_INCORRECT_DATA;
    }
    chain_id_32 = U4BE(data, offset);
    offset += sizeof(chain_id_32);
    chain_id = chain_id_32;

    if (offset >= lc) {
        // no signature
        return SWO_INCORRECT_DATA;
    }

    // sanity checks
    if ((ticker_length + 1) > sizeof(node->info.ticker)) {
        return SWO_INCORRECT_DATA;
    }
    if (decimals > UINT8_MAX) {
        PRINTF("Error: decimals received does not fit on one byte!\n");
        return false;
    }
    if (!app_compatible_with_chain_id(&chain_id)) {
        UNSUPPORTED_CHAIN_ID_MSG(chain_id);
        return SWO_INCORRECT_DATA;
    }

    // signature is computed on everything but the ticker length
    cx_hash_sha256(&data[1], offset - 1, hash, CX_SHA256_SIZE);
    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    LEDGER_SIGNATURE_PUBLIC_KEY,
                                    sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
                                    &data[offset],
                                    lc - offset) != true) {
        return SWO_INCORRECT_DATA;
    }

    // look for an existing matching node
    for (node = g_token_info_list; node != NULL;
         node = (s_token_info_node *) ((flist_node_t *) node)->next) {
        if (chain_id == node->info.chain_id) {
            if (memcmp(addr, node->info.address, sizeof(node->info.address)) == 0) {
                break;
            }
        }
        index += 1;
    }

    if (node == NULL) {
        if ((node = APP_MEM_ALLOC(sizeof(*node))) == NULL) {
            return SWO_INSUFFICIENT_MEMORY;
        }
        explicit_bzero(node, sizeof(*node));

        memcpy(node->info.address, addr, sizeof(node->info.address));
        node->info.chain_id = chain_id;
        flist_push_back((flist_node_t **) &g_token_info_list, (flist_node_t *) node);
    }

    memcpy(node->info.ticker, ticker, ticker_length);
    node->info.ticker[ticker_length] = '\0';
    node->info.decimals = (uint8_t) decimals;

    G_io_tx_buffer[0] = index;
    *tx += 1;
    return SWO_SUCCESS;
}

static void delete_token_info(s_token_info_node *node) {
    APP_MEM_FREE(node);
}

void clear_token_infos(void) {
    flist_clear((flist_node_t **) &g_token_info_list, (f_list_node_del) &delete_token_info);
}
