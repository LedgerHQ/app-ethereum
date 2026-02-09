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

#define TYPE_LESM_ACCOUNT_INFO 0x27
#define STRUCT_VERSION         0x01

#define MAX_THRESHOLD 100
#define MAX_SIGNERS   100

enum {
    TAG_STRUCTURE_TYPE = 0x01,
    TAG_STRUCTURE_VERSION = 0x02,
    TAG_CHALLENGE = 0x12,
    TAG_ADDRESS = 0x22,
    TAG_THRESHOLD = 0xa0,
    TAG_SIGNERS_COUNT = 0xa1,
    TAG_ROLE = 0xa2,
    TAG_DER_SIGNATURE = 0x15,
};

enum {
    BIT_STRUCTURE_TYPE,
    BIT_STRUCTURE_VERSION,
    BIT_CHALLENGE,
    BIT_ADDRESS,
    BIT_THRESHOLD,
    BIT_SIGNERS_COUNT,
    BIT_ROLE,
    BIT_DER_SIGNATURE,
};

typedef struct {
    safe_descriptor_t *safe;
    uint8_t sig_size;
    uint8_t *sig;
    cx_sha256_t hash_ctx;
    uint32_t rcv_flags;
} s_safe_ctx;

// Global structure to store the Safe Descriptor
safe_descriptor_t *SAFE_DESC = NULL;

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
        if (len > max) {                        \
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
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_struct_type(const s_tlv_data *data, s_safe_ctx *context) {
    CHECK_FIELD_LENGTH("STRUCTURE_TYPE", data->length, 1);
    CHECK_FIELD_VALUE("STRUCTURE_TYPE", data->value[0], TYPE_LESM_ACCOUNT_INFO);
    context->rcv_flags |= SET_BIT(BIT_STRUCTURE_TYPE);
    return true;
}

/**
 * @brief Handler for tag \ref STRUCTURE_VERSION.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_struct_version(const s_tlv_data *data, s_safe_ctx *context) {
    CHECK_FIELD_LENGTH("STRUCTURE_VERSION", data->length, 1);
    CHECK_FIELD_VALUE("STRUCTURE_VERSION", data->value[0], STRUCT_VERSION);
    context->rcv_flags |= SET_BIT(BIT_STRUCTURE_VERSION);
    return true;
}

/**
 * @brief Handler for tag \ref CHALLENGE.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_challenge(const s_tlv_data *data, s_safe_ctx *context) {
    uint8_t buf[sizeof(uint32_t)];

    CHECK_FIELD_LENGTH("CHALLENGE", data->length, sizeof(uint32_t));
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    context->rcv_flags |= SET_BIT(BIT_CHALLENGE);
    return check_challenge(read_u32_be(buf, 0));
}

/**
 * @brief Handler for tag \ref ADDRESS.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_address(const s_tlv_data *data, s_safe_ctx *context) {
    CHECK_FIELD_LENGTH("ADDRESS", data->length, ADDRESS_LENGTH);
    CHECK_EMPTY_BUFFER("ADDRESS", data->value, data->length);
    COPY_FIELD(context->safe->address, data);
    context->rcv_flags |= SET_BIT(BIT_ADDRESS);
    return true;
}

/**
 * @brief Handler for tag \ref THRESHOLD.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_threshold(const s_tlv_data *data, s_safe_ctx *context) {
    uint8_t buf[sizeof(context->safe->threshold)];
    CHECK_FIELD_OVERFLOW("THRESHOLD", data->length, sizeof(context->safe->threshold));
    if (data->length > sizeof(context->safe->threshold)) {
        PRINTF("THRESHOLD length must be 1 or 2 bytes\n");
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    context->safe->threshold = read_u16_be(buf, 0);
    if (context->safe->threshold == 0) {
        PRINTF("THRESHOLD cannot be 0\n");
        return false;
    }
    if (context->safe->threshold > MAX_THRESHOLD) {
        PRINTF("THRESHOLD cannot be greater than %d\n", MAX_THRESHOLD);
        return false;
    }
    context->rcv_flags |= SET_BIT(BIT_THRESHOLD);
    return true;
}

/**
 * @brief Handler for tag \ref SIGNERS_COUNT.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_signers_count(const s_tlv_data *data, s_safe_ctx *context) {
    uint8_t buf[sizeof(context->safe->signers_count)];
    CHECK_FIELD_OVERFLOW("SIGNERS_COUNT", data->length, sizeof(context->safe->signers_count));
    if (data->length > sizeof(context->safe->signers_count)) {
        PRINTF("SIGNERS_COUNT length must be 1 or 2 bytes\n");
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    context->safe->signers_count = read_u16_be(buf, 0);
    if (context->safe->signers_count == 0) {
        PRINTF("SIGNERS_COUNT cannot be 0\n");
        return false;
    }
    if (context->safe->signers_count > MAX_SIGNERS) {
        PRINTF("SIGNERS_COUNT cannot be greater than %d\n", MAX_SIGNERS);
        return false;
    }
    context->rcv_flags |= SET_BIT(BIT_SIGNERS_COUNT);
    return true;
}

/**
 * @brief Handler for tag \ref ROLE.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_role(const s_tlv_data *data, s_safe_ctx *context) {
    CHECK_FIELD_LENGTH("ROLE", data->length, sizeof(context->safe->role));
    context->safe->role = data->value[0];
    if (context->safe->role > ROLE_MAX) {
        PRINTF("ROLE cannot be greater than %d\n", ROLE_MAX);
        return false;
    }
    context->rcv_flags |= SET_BIT(BIT_ROLE);
    return true;
}

/**
 * Handler for tag \ref DER_SIGNATURE
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_signature(const s_tlv_data *data, s_safe_ctx *context) {
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
                                    NULL,
                                    0,
                                    CERTIFICATE_PUBLIC_KEY_USAGE_LES_MULTISIG,
                                    (uint8_t *) (context->sig),
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
    uint32_t expected_fields;

    expected_fields = (1 << BIT_STRUCTURE_TYPE) | (1 << BIT_STRUCTURE_VERSION) |
                      (1 << BIT_CHALLENGE) | (1 << BIT_ADDRESS) | (1 << BIT_THRESHOLD) |
                      (1 << BIT_SIGNERS_COUNT) | (1 << BIT_ROLE) | (1 << BIT_DER_SIGNATURE);

    return ((context->rcv_flags & expected_fields) == expected_fields);
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
 * @brief Parse the received Safe Descriptor TLV.
 *
 * @param[in] data the tlv data
 * @param[in] context Safe context
 * @return whether the handling was successful
 */
static bool handle_safe_tlv(const s_tlv_data *data, s_safe_ctx *context) {
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
        case TAG_THRESHOLD:
            ret = handle_threshold(data, context);
            break;
        case TAG_SIGNERS_COUNT:
            ret = handle_signers_count(data, context);
            break;
        case TAG_ROLE:
            ret = handle_role(data, context);
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
 * @brief Parse the TLV payload containing the Safe descriptor.
 *
 * @param[in] payload buffer received
 * @param[in] size of the buffer
 * @return whether the TLV payload was handled successfully or not
 */
bool handle_safe_tlv_payload(const uint8_t *payload, uint16_t size) {
    bool ret = false;
    s_safe_ctx ctx = {0};

    // Init the structure
    SAFE_DESC = app_mem_alloc(sizeof(safe_descriptor_t));
    if (SAFE_DESC == NULL) {
        PRINTF("Error: Memory allocation failed for Safe Descriptor!\n");
        return false;
    }
    explicit_bzero(SAFE_DESC, sizeof(safe_descriptor_t));
    ctx.safe = SAFE_DESC;
    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);

    ret = tlv_parse(payload, size, (f_tlv_data_handler) &handle_safe_tlv, &ctx);
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
