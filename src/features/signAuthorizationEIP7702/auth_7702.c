#include "auth_7702.h"
#include "utils.h"
#include "tlv_apdu.h"

#define STRUCT_VERSION 0x01

/**
 * @brief Parse the STRUCT_VERSION value.
 *
 * @param[in] data the tlv data
 * @param[in] context Auth 7702 context
 * @return whether the handling was successful
 */
static bool handle_version(const tlv_data_t *data, s_auth_7702_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_version(data, STRUCT_VERSION);
}

/**
 * @brief Parse the DELEGATE_ADDR value.
 *
 * @param[in] data the tlv data
 * @param[in] context Auth 7702 context
 * @return whether the handling was successful
 */
static bool handle_delegate_addr(const tlv_data_t *data, s_auth_7702_ctx *context) {
    return tlv_get_address(data, context->auth_7702.delegate, false);
}

/**
 * @brief Parse the CHAIN_ID value.
 *
 * @param[in] data the tlv data
 * @param[in] context Auth 7702 context
 * @return whether the handling was successful
 */
static bool handle_chain_id(const tlv_data_t *data, s_auth_7702_ctx *context) {
    return tlv_get_chain_id(data, &context->auth_7702.chainId);
}

/**
 * @brief Parse the NONCE value.
 *
 * @param[in] data the tlv data
 * @param[in] context Auth 7702 context
 * @return whether the handling was successful
 */
static bool handle_nonce(const tlv_data_t *data, s_auth_7702_ctx *context) {
    uint64_t value = 0;
    if (!get_uint64_t_from_tlv_data(data, &value)) {
        PRINTF("NONCE: failed to extract\n");
        return false;
    }
    context->auth_7702.nonce = value;
    return true;
}

// Define TLV tags for Auth 7702
#define AUTH_7702_TAGS(X)                                                \
    X(0x00, TAG_STRUCT_VERSION, handle_version, ENFORCE_UNIQUE_TAG)      \
    X(0x01, TAG_DELEGATE_ADDR, handle_delegate_addr, ENFORCE_UNIQUE_TAG) \
    X(0x02, TAG_CHAIN_ID, handle_chain_id, ENFORCE_UNIQUE_TAG)           \
    X(0x03, TAG_NONCE, handle_nonce, ENFORCE_UNIQUE_TAG)

// Generate TLV parser for Auth 7702
DEFINE_TLV_PARSER(AUTH_7702_TAGS, NULL, auth_7702_tlv_parser)

/**
 * @brief Wrapper to parse Auth 7702 TLV payload.
 *
 * @param[in] buf TLV buffer
 * @param[out] context Auth 7702 context
 * @return whether parsing was successful
 */
bool handle_auth_7702_tlv_payload(const buffer_t *buf, s_auth_7702_ctx *context) {
    return auth_7702_tlv_parser(buf, context, &context->received_tags);
}

bool verify_auth_7702_struct(const s_auth_7702_ctx *context) {
    return TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                   TAG_STRUCT_VERSION,
                                   TAG_DELEGATE_ADDR,
                                   TAG_CHAIN_ID,
                                   TAG_NONCE);
}
