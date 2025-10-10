#ifdef HAVE_SAFE_ACCOUNT

#include "signer_descriptor.h"
#include "safe_descriptor.h"
#include "apdu_constants.h"
#include "hash_bytes.h"
#include "public_keys.h"
#include "challenge.h"
#include "tlv.h"
#include "tlv_apdu.h"
#include "utils.h"
#include "nbgl_use_case.h"
#include "os_pki.h"
#include "ui_callbacks.h"
#include "signature.h"
#include "read.h"
#include "mem.h"

#define TYPE_VERIFIABLE_ADDRESS 0x0A
#define STRUCT_VERSION          0x01

enum {
    TAG_STRUCTURE_TYPE = 0x01,
    TAG_STRUCTURE_VERSION = 0x02,
    TAG_CHALLENGE = 0x12,
    TAG_ADDRESS = 0x22,
    TAG_DER_SIGNATURE = 0x15,
};

enum {
    BIT_STRUCTURE_TYPE,
    BIT_STRUCTURE_VERSION,
    BIT_CHALLENGE,
    BIT_ADDRESS,
    BIT_DER_SIGNATURE,
};

typedef struct {
    signers_descriptor_t *signers;
    uint8_t address_count;
    uint8_t sig_size;
    uint8_t *sig;
    cx_sha256_t hash_ctx;
    uint32_t rcv_flags;
} s_signer_ctx;

// Global structure to store the Signer Descriptor
signers_descriptor_t SIGNER_DESC = {0};

// Macros to check the field length
#define CHECK_FIELD_LENGTH(tag, len, expected)  \
    do {                                        \
        if (len != expected) {                  \
            PRINTF("%s Size mismatch!\n", tag); \
            return false;                       \
        }                                       \
    } while (0)
#define CHECK_FIELD_OVERFLOW(tag, len, max)     \
    do {                                        \
        if (len >= max) {                       \
            PRINTF("%s Size overflow!\n", tag); \
            return false;                       \
        }                                       \
    } while (0)

// Macro to check the field value
#define CHECK_FIELD_VALUE(tag, value, expected)  \
    do {                                         \
        if (value != expected) {                 \
            PRINTF("%s Value mismatch!\n", tag); \
            return false;                        \
        }                                        \
    } while (0)

// Macro to check the field value
#define CHECK_EMPTY_BUFFER(tag, field, len)   \
    do {                                      \
        uint8_t empty[ADDRESS_LENGTH] = {0};  \
        if (memcmp(field, empty, len) == 0) { \
            PRINTF("%s Zero buffer!\n", tag); \
            return false;                     \
        }                                     \
    } while (0)

// Macro to copy the field
#define COPY_FIELD(field, data)                             \
    do {                                                    \
        memmove((void *) field, data->value, data->length); \
    } while (0)

/**
 * @brief Handler for tag \ref STRUCTURE_TYPE.
 *
 * @param[in] data the tlv data
 * @param[in] context Signer context
 * @return whether the handling was successful
 */
static bool handle_struct_type(const s_tlv_data *data, s_signer_ctx *context) {
    CHECK_FIELD_LENGTH("STRUCTURE_TYPE", data->length, 1);
    CHECK_FIELD_VALUE("STRUCTURE_TYPE", data->value[0], TYPE_VERIFIABLE_ADDRESS);
    context->rcv_flags |= SET_BIT(BIT_STRUCTURE_TYPE);
    return true;
}

/**
 * @brief Handler for tag \ref STRUCTURE_VERSION.
 *
 * @param[in] data the tlv data
 * @param[in] context Signer context
 * @return whether the handling was successful
 */
static bool handle_struct_version(const s_tlv_data *data, s_signer_ctx *context) {
    CHECK_FIELD_LENGTH("STRUCTURE_VERSION", data->length, 1);
    CHECK_FIELD_VALUE("STRUCTURE_VERSION", data->value[0], STRUCT_VERSION);
    context->rcv_flags |= SET_BIT(BIT_STRUCTURE_VERSION);
    return true;
}

/**
 * @brief Handler for tag \ref CHALLENGE.
 *
 * @param[in] data the tlv data
 * @param[in] context Signer context
 * @return whether the handling was successful
 */
static bool handle_challenge(const s_tlv_data *data, s_signer_ctx *context) {
    uint8_t buf[sizeof(uint32_t)];

    CHECK_FIELD_LENGTH("CHALLENGE", data->length, sizeof(uint32_t));
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    context->rcv_flags |= SET_BIT(BIT_CHALLENGE);
    return (read_u32_be(buf, 0) == get_challenge());
}

/**
 * @brief Handler for tag \ref ADDRESS.
 *
 * @param[in] data the tlv data
 * @param[in] context Signer context
 * @return whether the handling was successful
 */
static bool handle_address(const s_tlv_data *data, s_signer_ctx *context) {
    CHECK_FIELD_LENGTH("ADDRESS", data->length, ADDRESS_LENGTH);
    CHECK_EMPTY_BUFFER("ADDRESS", data->value, data->length);
    if (context->address_count >= SAFE_DESC->signers_count) {
        PRINTF("Error: Too many addresses in Signer descriptor!\n");
        return false;
    }
    COPY_FIELD(context->signers->data[context->address_count++].address, data);
    context->rcv_flags |= SET_BIT(BIT_ADDRESS);
    return true;
}

/**
 * Handler for tag \ref DER_SIGNATURE
 *
 * @param[in] data the tlv data
 * @param[in] context Signer context
 * @return whether the handling was successful
 */
static bool handle_signature(const s_tlv_data *data, s_signer_ctx *context) {
    CHECK_FIELD_OVERFLOW("DER_SIGNATURE", data->length, ECDSA_SIGNATURE_MAX_LENGTH);
    context->sig_size = data->length;
    context->sig = (uint8_t *) data->value;
    context->rcv_flags |= SET_BIT(BIT_DER_SIGNATURE);
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
    cx_err_t error = CX_INTERNAL_ERROR;
    bool ret_code = false;

    CX_CHECK(
        cx_hash_no_throw((cx_hash_t *) &context->hash_ctx, CX_LAST, NULL, 0, hash, INT256_LENGTH));

    CX_CHECK(check_signature_with_pubkey("Signer descriptor",
                                         hash,
                                         sizeof(hash),
                                         NULL,
                                         0,
                                         CERTIFICATE_PUBLIC_KEY_USAGE_LES_MULTISIG,
                                         (uint8_t *) (context->sig),
                                         context->sig_size));

    ret_code = true;
end:
    return ret_code;
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
    uint32_t expected_fields;

    expected_fields = (1 << BIT_STRUCTURE_TYPE) | (1 << BIT_STRUCTURE_VERSION) |
                      (1 << BIT_CHALLENGE) | (1 << BIT_ADDRESS) | (1 << BIT_DER_SIGNATURE);

    return ((context->rcv_flags & expected_fields) == expected_fields);
}

/**
 * @brief Print the Signer descriptor.
 *
 * @param[in] context Signer context
 * Only for debug purpose.
 */
static void print_signer_info(const s_signer_ctx *context) {
    uint8_t i = 0;
    UNUSED(context);

    PRINTF("****************************************************************************\n");
    PRINTF("[SAFE ACCOUNT] - Retrieved Signer Descriptor:\n");
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
 * @brief Parse the received Signer Descriptor TLV.
 *
 * @param[in] data the tlv data
 * @param[in] context Signer context
 * @return whether the handling was successful
 */
static bool handle_signer_tlv(const s_tlv_data *data, s_signer_ctx *context) {
    bool ret = false;

    switch (data->tag) {
        case TAG_STRUCTURE_TYPE:
            ret = handle_struct_type(data, context);
            break;
        case TAG_STRUCTURE_VERSION:
            ret = handle_struct_version(data, context);
            break;
        case TAG_CHALLENGE:
            ret = handle_challenge(data, context);
            break;
        case TAG_ADDRESS:
            ret = handle_address(data, context);
            break;
        case TAG_DER_SIGNATURE:
            ret = handle_signature(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            break;
    }
    if ((ret == true) && (data->tag != TAG_DER_SIGNATURE)) {
        hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    }
    return ret;
}

/**
 * @brief Parse the TLV payload containing the Signer descriptor.
 *
 * @param[in] payload buffer received
 * @param[in] size of the buffer
 * @return whether the TLV payload was handled successfully or not
 */
bool handle_signer_tlv_payload(const uint8_t *payload, uint16_t size) {
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
    SIGNER_DESC.data = app_mem_alloc(SAFE_DESC->signers_count * sizeof(signer_data_t));
    if (SIGNER_DESC.data == NULL) {
        PRINTF("Error: Memory allocation failed for Signer Descriptor!\n");
        return false;
    }
    explicit_bzero(SIGNER_DESC.data, SAFE_DESC->signers_count * sizeof(signer_data_t));
    SIGNER_DESC.is_valid = false;
    ctx.signers = &SIGNER_DESC;
    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);

    ret = tlv_parse(payload, size, (f_tlv_data_handler) &handle_signer_tlv, &ctx);
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
    if (SIGNER_DESC.data != NULL) {
        app_mem_free(SIGNER_DESC.data);
    }
    explicit_bzero(&SIGNER_DESC, sizeof(SIGNER_DESC));
}

#endif  // HAVE_SAFE_ACCOUNT
