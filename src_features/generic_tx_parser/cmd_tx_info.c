#ifdef HAVE_GENERIC_TX_PARSER

#include "cmd_tx_info.h"
#include "cx.h"
#include "apdu_constants.h"
#include "mem.h"
#include "public_keys.h" // LEDGER_SIGNATURE_PUBLIC_KEY
#include "gtp_tx_info.h"
#include "read.h" // read_u*_be
#include "gtp_field_table.h"

static uint16_t g_tlv_size;
static uint16_t g_tlv_pos;
static uint8_t *g_tlv_payload;
extern const s_tx_info *g_tx_info;
extern cx_sha3_t hash_ctx;

static bool verify_struct(const s_tx_info_ctx *context) {
    uint16_t required_bits = 0;
    uint8_t hash[INT256_LENGTH];

    // check if struct version was provided
    required_bits |= SET_BIT(BIT_VERSION);
    if ((context->set_flags & required_bits) != required_bits) {
        PRINTF("Error: no struct version specified!\n");
        return false;
    }

    // verify required fields
    switch (context->tx_info->version) {
        case 1:
            required_bits |= (SET_BIT(BIT_CHAIN_ID) |
                              SET_BIT(BIT_CONTRACT_ADDR) |
                              SET_BIT(BIT_SELECTOR) |
                              SET_BIT(BIT_FIELDS_HASH) |
                              SET_BIT(BIT_OPERATION_TYPE) |
                              SET_BIT(BIT_SIGNATURE));
            break;
        default:
            PRINTF("Error: unsupported tx info version (%u)\n", context->tx_info->version);
            return false;
    }

    if ((context->set_flags & required_bits) != required_bits) {
        PRINTF("Error: missing required field(s)\n");
        return false;
    }

    // verify signature
    if (cx_hash_no_throw((cx_hash_t *) &context->struct_hash, CX_LAST, NULL, 0, hash, sizeof(hash)) != CX_OK) {
        PRINTF("Could not finalize struct hash!\n");
        return false;
    }

    // TODO: change to LEDGER_CALLDATA_DESCRIPTOR key once available
    if (check_signature_with_pubkey("TX info",
                                    hash,
                                    sizeof(hash),
                                    LEDGER_SIGNATURE_PUBLIC_KEY,
                                    sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
#ifdef HAVE_LEDGER_PKI
                                    CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
#endif
                                    (uint8_t *) context->tx_info->signature,
                                    context->tx_info->signature_len) != CX_OK) {
        return false;
    }
    return true;
}

uint16_t handle_tx_info(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    uint8_t offset = 0;
    s_tx_info_ctx ctx = {0};

    (void)p2;
    if (appState != APP_STATE_SIGNING_TX) {
        PRINTF("App not in TX signing mode!\n");
        return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }
    switch (p1) {
        case P1_FIRST_CHUNK:
            if ((ctx.tx_info = mem_alloc(sizeof(*ctx.tx_info))) == NULL) {
                return false;
            }
            explicit_bzero(ctx.tx_info, sizeof(*ctx.tx_info));
            if ((offset + sizeof(g_tlv_size)) > lc) {
                return APDU_RESPONSE_INVALID_DATA;
            }
            g_tlv_size = read_u16_be(payload, offset);
            offset += sizeof(g_tlv_size);
            g_tlv_pos = 0;
            if ((g_tlv_payload = mem_alloc(g_tlv_size)) == NULL) {
                return APDU_RESPONSE_INSUFFICIENT_MEMORY;
            }
            __attribute__((fallthrough));
        case P1_FOLLOWING_CHUNK:
            memcpy(g_tlv_payload + g_tlv_pos, payload + offset, lc - offset);
            g_tlv_pos += (lc - offset);
            break;
    }
    if (g_tlv_pos > g_tlv_size) {
        PRINTF("TLV payload bigger than expected!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    if (g_tlv_pos == g_tlv_size) {
        cx_sha256_init((cx_sha256_t*)&ctx.struct_hash);
        if (!parse_tlv(g_tlv_payload, g_tlv_size, (f_tlv_data_handler)&handle_tx_info_struct, &ctx)) return APDU_RESPONSE_INVALID_DATA;
        mem_dealloc(sizeof(g_tlv_size));
        if (!verify_struct(&ctx)) {
            return APDU_RESPONSE_INVALID_DATA;
        }
        if (cx_sha3_init_no_throw(&hash_ctx, 256) != CX_OK) {
            return APDU_RESPONSE_UNKNOWN;
        }
        g_tx_info = ctx.tx_info;
        field_table_init();
    }
    return APDU_RESPONSE_OK;
}

#endif // HAVE_GENERIC_TX_PARSER
