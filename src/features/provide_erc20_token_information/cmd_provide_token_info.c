#include "apdu_constants.h"
#include "tlv_use_case_dynamic_descriptor.h"
#include "public_keys.h"
#include "network.h"
#include "os_pki.h"
#include "app_mem_utils.h"
#include "lists.h"
#include "token_info.h"
#include "utils.h"
#include "tlv_apdu.h"

typedef struct {
    flist_node_t _list;
    s_token_info info;
} s_token_info_node;

static s_token_info_node *g_token_info_list;

static bool erc20_token_info_common(const uint64_t *chain_id,
                                    const uint8_t *address,
                                    size_t ticker_length,
                                    const char *ticker,
                                    uint8_t decimals) {
    s_token_info_node *node;
    uint8_t index = 0;

    // sanity checks
    if (!app_compatible_with_chain_id(chain_id)) {
        PRINTF("Error: unsupported chain ID! (%llu)\n", *chain_id);
        return false;
    }
    if ((ticker_length + 1) > sizeof(node->info.ticker)) {
        PRINTF("Error: token ticker too long!\n");
        return false;
    }

    // look for an existing matching node
    for (node = g_token_info_list; node != NULL;
         node = (s_token_info_node *) ((flist_node_t *) node)->next) {
        if (*chain_id == node->info.chain_id) {
            if (memcmp(address, node->info.address, sizeof(node->info.address)) == 0) {
                break;
            }
        }
        index += 1;
    }

    if (node == NULL) {
        if ((node = APP_MEM_ALLOC(sizeof(*node))) == NULL) {
            return false;
        }
        explicit_bzero(node, sizeof(*node));

        memcpy(node->info.address, address, sizeof(node->info.address));
        node->info.chain_id = *chain_id;
        flist_push_back((flist_node_t **) &g_token_info_list, (flist_node_t *) node);
    }

    memcpy(node->info.ticker, ticker, ticker_length);
    node->info.ticker[ticker_length] = '\0';
    node->info.decimals = decimals;

    G_io_tx_buffer[0] = index;
    return true;
}

static bool erc20_token_info_handler_legacy(uint8_t lc, const uint8_t *data) {
    uint32_t offset = 0;
    s_token_info_node *node;
    uint8_t ticker_length;
    const char *ticker;
    const uint8_t *addr;
    uint32_t decimals;
    uint32_t chain_id_32;
    uint64_t chain_id;
    uint8_t hash[INT256_LENGTH];

    if ((offset + sizeof(ticker_length)) > lc) {
        return false;
    }
    ticker_length = data[offset];
    offset += sizeof(ticker_length);

    if ((offset + ticker_length) > lc) {
        return false;
    }
    ticker = (const char *) &data[offset];
    offset += ticker_length;

    if ((offset + sizeof(node->info.address)) > lc) {
        return false;
    }
    addr = &data[offset];
    offset += sizeof(node->info.address);

    // TODO: 4 bytes for this is overkill
    if ((offset + sizeof(decimals)) > lc) {
        return false;
    }
    decimals = U4BE(data, offset);
    offset += sizeof(decimals);

    // TODO: Handle 64-bit long chain IDs
    if ((offset + sizeof(chain_id_32)) > lc) {
        return false;
    }
    chain_id_32 = U4BE(data, offset);
    offset += sizeof(chain_id_32);
    chain_id = chain_id_32;

    if (offset >= lc) {
        // no signature
        return false;
    }

    // sanity checks
    if (decimals > UINT8_MAX) {
        PRINTF("Error: decimals received does not fit on one byte!\n");
        return false;
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
        return false;
    }

    return erc20_token_info_common(&chain_id, addr, ticker_length, ticker, (uint8_t) decimals);
}

typedef struct {
    TLV_reception_t received_tags;
    uint64_t chain_id;
    buffer_t address;
} s_tuid_ctx;

static bool handle_tuid_chain_id(const tlv_data_t *data, s_tuid_ctx *out) {
    return get_uint64_t_from_tlv_data(data, &out->chain_id);
}

static bool handle_tuid_address(const tlv_data_t *data, s_tuid_ctx *out) {
    return get_buffer_from_tlv_data(data, &out->address, 1, ADDRESS_LENGTH);
}

#define TUID_TLV_TAGS(X)                                            \
    X(0x23, TAG_CHAIN_ID, handle_tuid_chain_id, ENFORCE_UNIQUE_TAG) \
    X(0x22, TAG_ADDRESS, handle_tuid_address, ENFORCE_UNIQUE_TAG)

DEFINE_TLV_PARSER(TUID_TLV_TAGS, NULL, parse_dynamic_token_tuid);

static bool erc20_token_info_handler(const buffer_t *buf) {
    tlv_dynamic_descriptor_out_t tlv_output = {0};

    if (tlv_use_case_dynamic_descriptor(buf, &tlv_output) != TLV_DYNAMIC_DESCRIPTOR_SUCCESS) {
        return false;
    }

    // sanity checks
    if (tlv_output.coin_type != g_chain_config->coin_type) {
        PRINTF("Error: expected coin type %u, got %u.\n",
               g_chain_config->coin_type,
               tlv_output.coin_type);
        return false;
    }

    // parse TUID
    s_tuid_ctx out = {0};
    uint8_t addr_buf[ADDRESS_LENGTH];
    if (!parse_dynamic_token_tuid(&tlv_output.TUID, &out, &out.received_tags)) {
        return false;
    }
    if (!TLV_CHECK_RECEIVED_TAGS(out.received_tags, TAG_CHAIN_ID, TAG_ADDRESS)) {
        return false;
    }
    buf_shrink_expand(out.address.ptr, out.address.size, addr_buf, sizeof(addr_buf));

    return erc20_token_info_common(&out.chain_id,
                                   addr_buf,
                                   strlen(tlv_output.ticker),
                                   tlv_output.ticker,
                                   tlv_output.magnitude);
}

uint16_t handle_provide_erc20_token_information(uint8_t p1,
                                                uint8_t p2,
                                                uint8_t lc,
                                                const uint8_t *data,
                                                unsigned int *tx) {
    e_tlv_apdu_ret ret;

    switch (p1) {
        case 0:
            if (p2 != 0) {
                return SWO_INCORRECT_P1_P2;
            }
            if (!erc20_token_info_handler_legacy(lc, data)) {
                return SWO_INCORRECT_DATA;
            }
            *tx += 1;
            break;

        case 1:
            ret = tlv_from_apdu(p2 != 0, lc, data, &erc20_token_info_handler);
            if (ret == TLV_APDU_ERROR) {
                return SWO_INCORRECT_DATA;
            }
            if (ret == TLV_APDU_SUCCESS) {
                *tx += 1;
            }
            break;

        default:
            return SWO_INCORRECT_P1_P2;
    }
    return SWO_SUCCESS;
}

static void delete_token_info(s_token_info_node *node) {
    APP_MEM_FREE(node);
}

void clear_token_infos(void) {
    flist_clear((flist_node_t **) &g_token_info_list, (f_list_node_del) &delete_token_info);
}

const s_token_info *get_matching_token_info(const uint64_t *chain_id, const uint8_t *address) {
    for (const s_token_info_node *node = g_token_info_list; node != NULL;
         node = (s_token_info_node *) ((flist_node_t *) node)->next) {
        if (*chain_id == node->info.chain_id) {
            if (memcmp(address, node->info.address, sizeof(node->info.address)) == 0) {
                return &node->info;
            }
        }
    }
    return NULL;
}
