#include "tlv_use_case_dynamic_descriptor.h"
#include "apdu_constants.h"
#include "public_keys.h"
#include "network.h"
#include "os_pki.h"
#include "token_info.h"
#include "utils.h"
#include "tlv_apdu.h"

static bool erc20_token_info_common(const uint64_t *chain_id,
                                    const uint8_t *address,
                                    size_t ticker_length,
                                    const char *ticker,
                                    uint8_t decimals) {
    s_token_info info = {0};
    int index;

    if ((ticker_length + 1) > sizeof(info.ticker)) {
        return false;
    }
    // Reject it if this app could never sign a transaction on that chain.
    if (!chain_id_is_signable(*chain_id)) {
        UNSUPPORTED_CHAIN_ID_MSG(*chain_id);
        return false;
    }
    memcpy(info.address, address, sizeof(info.address));
    memcpy(info.ticker, ticker, ticker_length);
    info.decimals = decimals;
    info.chain_id = *chain_id;

    if ((index = set_token_info(&info)) == -1) {
        PRINTF("Error: could not set token info!\n");
        return false;
    }
    if (index > UINT8_MAX) {
        // Could not represent it on one byte
        return false;
    }

    G_io_tx_buffer[0] = (uint8_t) index;
    return true;
}

static bool erc20_token_info_handler_legacy(uint8_t lc, const uint8_t *data) {
    uint32_t offset = 0;
    uint8_t ticker_length;
    const char *ticker;
    const uint8_t *address;
    uint32_t decimals_32;
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

    if ((offset + ADDRESS_LENGTH) > lc) {
        return false;
    }
    address = &data[offset];
    offset += ADDRESS_LENGTH;

    // TODO: 4 bytes for this is overkill
    if ((offset + sizeof(decimals_32)) > lc) {
        return false;
    }
    decimals_32 = U4BE(data, offset);
    offset += sizeof(decimals_32);

    // TODO: Handle 64-bit long chain IDs
    if ((offset + sizeof(chain_id_32)) > lc) {
        return false;
    }
    chain_id_32 = U4BE(data, offset);
    chain_id = (uint64_t) chain_id_32;
    offset += sizeof(chain_id_32);

    if (offset >= lc) {
        // no signature
        return false;
    }

    // sanity checks
    if (decimals_32 > UINT8_MAX) {
        PRINTF("Error: decimals received does not fit on one byte!\n");
        return false;
    }

    // signature is computed on everything but the ticker length
    cx_hash_sha256(&data[sizeof(ticker_length)],
                   offset - sizeof(ticker_length),
                   hash,
                   CX_SHA256_SIZE);
    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    LEDGER_SIGNATURE_PUBLIC_KEY,
                                    sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
                                    &data[offset],
                                    lc - offset) != true) {
        return false;
    }

    return erc20_token_info_common(&chain_id,
                                   address,
                                   ticker_length,
                                   ticker,
                                   (uint8_t) decimals_32);
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

DEFINE_TLV_PARSER(TUID_TLV_TAGS, NULL, parse_dynamic_token_tuid)

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
            switch (tlv_from_apdu(p2 != 0, lc, data, &erc20_token_info_handler)) {
                case TLV_APDU_ERROR:
                    return SWO_INCORRECT_DATA;

                case TLV_APDU_SUCCESS:
                    *tx += 1;
                    break;

                case TLV_APDU_PENDING:
                    break;
            }
            break;

        default:
            return SWO_INCORRECT_P1_P2;
    }
    return SWO_SUCCESS;
}
