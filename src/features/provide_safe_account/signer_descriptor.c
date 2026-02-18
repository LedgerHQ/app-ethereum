#include "signer_descriptor.h"
#include "safe_descriptor.h"
#include "challenge.h"
#include "hash_bytes.h"
#include "app_mem_utils.h"
#include "mem_utils.h"
#include "public_keys.h"
#include "signature.h"
#include "tlv_library.h"
#include "tlv_apdu.h"
#include "utils.h"

#define TYPE_VERIFIABLE_ADDRESS 0x0A
#define STRUCT_VERSION          0x01

typedef struct {
    signers_descriptor_t *signers;
    uint8_t address_count;
    uint8_t sig_size;
    const uint8_t *sig;
    cx_sha256_t hash_ctx;
    TLV_reception_t received_tags;
} s_signer_ctx;

// Global structure to store the Signer Descriptor
signers_descriptor_t SIGNER_DESC = {0};

/**
 * @brief Handler for tag \ref STRUCTURE_TYPE.
 *
 * @param[in] data the tlv data
 * @param[in] context Signer context
 * @return whether the handling was successful
 */
static bool handle_struct_type(const tlv_data_t *data, s_signer_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_type(data, TYPE_VERIFIABLE_ADDRESS);
}

/**
 * @brief Handler for tag \ref STRUCTURE_VERSION.
 *
 * @param[in] data the tlv data
 * @param[in] context Signer context
 * @return whether the handling was successful
 */
static bool handle_struct_version(const tlv_data_t *data, s_signer_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_version(data, STRUCT_VERSION);
}

/**
 * @brief Handler for tag \ref CHALLENGE.
 *
 * @param[in] data the tlv data
 * @param[in] context Signer context
 * @return whether the handling was successful
 */
static bool handle_challenge(const tlv_data_t *data, s_signer_ctx *context) {
    UNUSED(context);
    return tlv_check_challenge(data);
}

/**
 * @brief Handler for tag \ref ADDRESS.
 *
 * @param[in] data the tlv data
 * @param[in] context Signer context
 * @return whether the handling was successful
 */
static bool handle_address(const tlv_data_t *data, s_signer_ctx *context) {
    // Verify bounds BEFORE writing to buffer
    if (context->address_count >= SAFE_DESC->signers_count) {
        PRINTF("Error: Too many addresses in Signer descriptor!\n");
        return false;
    }

    if (tlv_get_address(data,
                        (uint8_t *) context->signers->data[context->address_count].address,
                        true) == false) {
        return false;
    }
    context->address_count++;
    return true;
}

/**
 * Handler for tag \ref DER_SIGNATURE
 *
 * @param[in] data the tlv data
 * @param[in] context Signer context
 * @return whether the handling was successful
 */
static bool handle_signature(const tlv_data_t *data, s_signer_ctx *context) {
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

// Define TLV tags for Signer descriptor
#define SIGNER_TAGS(X)                                                        \
    X(0x01, TAG_STRUCTURE_TYPE, handle_struct_type, ENFORCE_UNIQUE_TAG)       \
    X(0x02, TAG_STRUCTURE_VERSION, handle_struct_version, ENFORCE_UNIQUE_TAG) \
    X(0x12, TAG_CHALLENGE, handle_challenge, ENFORCE_UNIQUE_TAG)              \
    X(0x22, TAG_ADDRESS, handle_address, ALLOW_MULTIPLE_TAG)                  \
    X(0x15, TAG_DER_SIGNATURE, handle_signature, ENFORCE_UNIQUE_TAG)

// Forward declaration of common handler
static bool signer_common_handler(const tlv_data_t *data, s_signer_ctx *context);

// Generate TLV parser for Signer descriptor
DEFINE_TLV_PARSER(SIGNER_TAGS, &signer_common_handler, signer_tlv_parser)

/**
 * @brief Common handler that hashes all tags except signature
 *
 * @param[in] data the tlv data
 * @param[in] context Signer context
 * @return true on success
 */
static bool signer_common_handler(const tlv_data_t *data, s_signer_ctx *context) {
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
 * @param[in] context Signer context
 * @return whether it was successful
 */
static bool verify_signature(const s_signer_ctx *context) {
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
 * @param[in] context Signer context
 * @return whether it was successful
 */
static bool verify_fields(const s_signer_ctx *context) {
    return TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                   TAG_STRUCTURE_TYPE,
                                   TAG_STRUCTURE_VERSION,
                                   TAG_CHALLENGE,
                                   TAG_ADDRESS,
                                   TAG_DER_SIGNATURE);
}

/**
 * @brief Print the Signer descriptor.
 *
 * @param[in] context Signer context
 * Only for debug purpose.
 */
static void print_signer_info(const s_signer_ctx *context) {
    uint8_t i = 0;

    PRINTF("****************************************************************************\n");
    PRINTF("[SAFE ACCOUNT] - Retrieved Signer Descriptor [%d]:\n", context->address_count);
    for (i = 0; i < context->address_count; i++) {
        PRINTF("[SAFE ACCOUNT] -    Address[%d]: %.*h\n",
               i,
               ADDRESS_LENGTH,
               context->signers->data[i].address);
    }
}

/**
 * @brief Verify the struct
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] context Signer context
 * @return whether it was successful
 */
static bool verify_signer_struct(const s_signer_ctx *context) {
    if (!verify_fields(context)) {
        PRINTF("Error: Missing mandatory fields in Signer descriptor!\n");
        return false;
    }
    if (!verify_signature(context)) {
        PRINTF("Error: Signature verification failed for Signer descriptor!\n");
        return false;
    }
    if (context->address_count < SAFE_DESC->signers_count) {
        PRINTF("Error: Too few addresses in Signer descriptor!\n");
        return false;
    }
    context->signers->is_valid = true;
    print_signer_info(context);
    return true;
}

/**
 * @brief Parse the TLV payload containing the Signer descriptor.
 *
 * @param[in] payload buffer_t with the complete TLV payload
 * @return whether the TLV payload was handled successfully or not
 */
bool handle_signer_tlv_payload(const buffer_t *payload) {
    bool ret = false;
    s_signer_ctx ctx = {0};

    if (SAFE_DESC == NULL) {
        PRINTF("Error: Safe descriptor doesn't exist!\n");
        return false;
    }
    if (SAFE_DESC->signers_count == 0) {
        PRINTF("Error: Safe descriptor doesn't contain any Signers!\n");
        return false;
    }
    if (SIGNER_DESC.data != NULL) {
        PRINTF("Error: Signer descriptors already exist!\n");
        return false;
    }
    // Init the structure
    if (APP_MEM_CALLOC((void **) &SIGNER_DESC.data,
                       SAFE_DESC->signers_count * sizeof(signer_data_t)) == false) {
        PRINTF("Error: Memory allocation failed for Signer Descriptor!\n");
        return false;
    }
    SIGNER_DESC.is_valid = false;
    ctx.signers = &SIGNER_DESC;
    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);

    // Parse using SDK TLV parser
    ret = signer_tlv_parser(payload, &ctx, &ctx.received_tags);
    if (ret) {
        ret = verify_signer_struct(&ctx);
    }
    if (!ret) {
        clear_signer_descriptor();
    }
    return ret;
}

/**
 * @brief Clear the Signer memory.
 *
 */
void clear_signer_descriptor(void) {
    APP_MEM_FREE(SIGNER_DESC.data);
    explicit_bzero(&SIGNER_DESC, sizeof(SIGNER_DESC));
}
