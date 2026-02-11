#include "safe_descriptor.h"
#include "challenge.h"
#include "hash_bytes.h"
#include "mem.h"
#include "mem_utils.h"
#include "public_keys.h"
#include "signature.h"
#include "tlv_library.h"
#include "tlv_apdu.h"
#include "utils.h"

#define TYPE_LESM_ACCOUNT_INFO 0x27
#define STRUCT_VERSION         0x01

#define MAX_THRESHOLD 100
#define MAX_SIGNERS   100

typedef struct {
    safe_descriptor_t *safe;
    uint8_t sig_size;
    const uint8_t *sig;
    cx_sha256_t hash_ctx;
    TLV_reception_t received_tags;
} s_safe_ctx;

// Global structure to store the Safe Descriptor
safe_descriptor_t *SAFE_DESC = NULL;

/**
 * @brief Handler for tag \ref STRUCTURE_TYPE.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_struct_type(const tlv_data_t *data, s_safe_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_type(data, TYPE_LESM_ACCOUNT_INFO);
}

/**
 * @brief Handler for tag \ref STRUCTURE_VERSION.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_struct_version(const tlv_data_t *data, s_safe_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_version(data, STRUCT_VERSION);
}

/**
 * @brief Handler for tag \ref CHALLENGE.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_challenge(const tlv_data_t *data, s_safe_ctx *context) {
    UNUSED(context);
    return tlv_check_challenge(data);
}

/**
 * @brief Handler for tag \ref ADDRESS.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_address(const tlv_data_t *data, s_safe_ctx *context) {
    return tlv_get_address(data, (uint8_t *) context->safe->address, true);
}

/**
 * @brief Handler for tag \ref THRESHOLD.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_threshold(const tlv_data_t *data, s_safe_ctx *context) {
    if (!tlv_get_uint16(data, &context->safe->threshold, 1, MAX_THRESHOLD)) {
        PRINTF("THRESHOLD: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Handler for tag \ref SIGNERS_COUNT.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_signers_count(const tlv_data_t *data, s_safe_ctx *context) {
    if (!tlv_get_uint16(data, &context->safe->signers_count, 1, MAX_SIGNERS)) {
        PRINTF("SIGNERS_COUNT: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Handler for tag \ref ROLE.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_role(const tlv_data_t *data, s_safe_ctx *context) {
    if (!tlv_get_uint8(data, &context->safe->role, 0, ROLE_MAX)) {
        PRINTF("ROLE: error\n");
        return false;
    }
    return true;
}

/**
 * Handler for tag \ref DER_SIGNATURE
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_signature(const tlv_data_t *data, s_safe_ctx *context) {
    buffer_t sig = {0};
    if (!get_buffer_from_tlv_data(data,
                                  &sig,
                                  ECDSA_SIGNATURE_MIN_LENGTH,
                                  ECDSA_SIGNATURE_MAX_LENGTH)) {
        PRINTF("DER_SIGNATURE: failed to extract\n");
        return false;
    }
    context->sig_size = sig.size;
    context->sig = sig.ptr;
    return true;
}

// Define TLV tags for Safe descriptor
#define SAFE_TAGS(X)                                                          \
    X(0x01, TAG_STRUCTURE_TYPE, handle_struct_type, ENFORCE_UNIQUE_TAG)       \
    X(0x02, TAG_STRUCTURE_VERSION, handle_struct_version, ENFORCE_UNIQUE_TAG) \
    X(0x12, TAG_CHALLENGE, handle_challenge, ENFORCE_UNIQUE_TAG)              \
    X(0x22, TAG_ADDRESS, handle_address, ENFORCE_UNIQUE_TAG)                  \
    X(0xa0, TAG_THRESHOLD, handle_threshold, ENFORCE_UNIQUE_TAG)              \
    X(0xa1, TAG_SIGNERS_COUNT, handle_signers_count, ENFORCE_UNIQUE_TAG)      \
    X(0xa2, TAG_ROLE, handle_role, ENFORCE_UNIQUE_TAG)                        \
    X(0x15, TAG_DER_SIGNATURE, handle_signature, ENFORCE_UNIQUE_TAG)

// Forward declaration of common handler
static bool safe_common_handler(const tlv_data_t *data, s_safe_ctx *context);

// Generate TLV parser for Safe descriptor
DEFINE_TLV_PARSER(SAFE_TAGS, &safe_common_handler, safe_tlv_parser)

/**
 * @brief Common handler that hashes all tags except signature
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return true on success
 */
static bool safe_common_handler(const tlv_data_t *data, s_safe_ctx *context) {
    // Hash all tags except the signature
    if (data->tag != TAG_DER_SIGNATURE) {
        hash_nbytes(data->raw.ptr, data->raw.size, (cx_hash_t *) &context->hash_ctx);
    }
    return true;
}

/**
 * @brief Verify the payload signature
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] context Safe context
 * @return whether it was successful
 */
static bool verify_signature(const s_safe_ctx *context) {
    uint8_t hash[INT256_LENGTH];

    if (finalize_hash((cx_hash_t *) &context->hash_ctx, hash, sizeof(hash)) != true) {
        return false;
    }

    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_LES_MULTISIG,
                                    (uint8_t *) context->sig,
                                    context->sig_size) != true) {
        return false;
    }

    return true;
}

/**
 * @brief Verify the received fields
 *
 * Check the mandatory fields are present
 *
 * @param[in] context Safe context
 * @return whether it was successful
 */
static bool verify_fields(const s_safe_ctx *context) {
    return TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                   TAG_STRUCTURE_TYPE,
                                   TAG_STRUCTURE_VERSION,
                                   TAG_CHALLENGE,
                                   TAG_ADDRESS,
                                   TAG_THRESHOLD,
                                   TAG_SIGNERS_COUNT,
                                   TAG_ROLE,
                                   TAG_DER_SIGNATURE);
}

/**
 * @brief Print the Safe descriptor.
 *
 * @param[in] context Safe context
 * Only for debug purpose.
 */
static void print_safe_info(const s_safe_ctx *context) {
    UNUSED(context);
    PRINTF("****************************************************************************\n");
    PRINTF("[SAFE ACCOUNT] - Retrieved Safe Descriptor:\n");
    PRINTF("[SAFE ACCOUNT] -    Address: %.*h\n", ADDRESS_LENGTH, context->safe->address);
    PRINTF("[SAFE ACCOUNT] -    Threshold: %d (0x%x)\n",
           context->safe->threshold,
           context->safe->threshold);
    PRINTF("[SAFE ACCOUNT] -    Signers count: %d (0x%x)\n",
           context->safe->signers_count,
           context->safe->signers_count);
    PRINTF("[SAFE ACCOUNT] -    Role: %d (%s)\n",
           context->safe->role,
           ROLE_STR(context->safe->role));
}

/**
 * @brief Verify the struct
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] context Safe context
 * @return whether it was successful
 */
static bool verify_safe_struct(const s_safe_ctx *context) {
    if (!verify_fields(context)) {
        PRINTF("Error: Missing mandatory fields in Safe descriptor!\n");
        return false;
    }

    if (!verify_signature(context)) {
        PRINTF("Error: Signature verification failed for Safe descriptor!\n");
        return false;
    }
    print_safe_info(context);
    return true;
}

/**
 * @brief Parse the TLV payload containing the Safe descriptor.
 *
 * @param[in] payload buffer_t with the complete TLV payload
 * @return whether the TLV payload was handled successfully or not
 */
bool handle_safe_tlv_payload(const buffer_t *payload) {
    bool ret = false;
    s_safe_ctx ctx = {0};

    // Init the structure
    if (!mem_buffer_allocate((void **) &SAFE_DESC, sizeof(safe_descriptor_t))) {
        PRINTF("Error: Memory allocation failed for Safe Descriptor!\n");
        return false;
    }
    ctx.safe = SAFE_DESC;
    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);

    // Parse using SDK TLV parser
    ret = safe_tlv_parser(payload, &ctx, &ctx.received_tags);
    if (ret) {
        ret = verify_safe_struct(&ctx);
    }
    if (!ret) {
        clear_safe_descriptor();
    }
    return ret;
}

/**
 * @brief Clear the Safe memory.
 *
 */
void clear_safe_descriptor(void) {
    app_mem_free(SAFE_DESC);
    SAFE_DESC = NULL;
}
