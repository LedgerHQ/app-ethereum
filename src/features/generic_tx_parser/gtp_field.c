#include <string.h>
#include "gtp_field.h"
#include "utils.h"
#include "os_print.h"
#include "shared_context.h"  // strings
#include "mem.h"             // app_mem_free
#include "mem_utils.h"       // mem_buffer_cleanup
#include "lists.h"
#include "tlv_library.h"
#include "tlv_apdu.h"

typedef union {
    s_param_raw_context raw_ctx;
    s_param_amount_context amount_ctx;
    s_param_token_amount_context token_amount_ctx;
    s_param_nft_context nft_ctx;
    s_param_datetime_context datetime_ctx;
    s_param_duration_context duration_ctx;
    s_param_unit_context unit_ctx;
    s_param_enum_context enum_ctx;
    s_param_trusted_name_context trusted_name_ctx;
    s_param_calldata_context calldata_ctx;
    s_param_token_context token_ctx;
    s_param_network_context network_ctx;
} u_param_context;

// Forward declarations
static bool handle_version(const tlv_data_t *data, s_field_ctx *context);
static bool handle_name(const tlv_data_t *data, s_field_ctx *context);
static bool handle_param_type(const tlv_data_t *data, s_field_ctx *context);
static bool handle_param(const tlv_data_t *data, s_field_ctx *context);
static bool handle_param_visible(const tlv_data_t *data, s_field_ctx *context);
static bool handle_param_constraint(const tlv_data_t *data, s_field_ctx *context);

// Define TLV tags for Field
#define FIELD_TAGS(X)                                              \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)       \
    X(0x01, TAG_NAME, handle_name, ENFORCE_UNIQUE_TAG)             \
    X(0x02, TAG_PARAM_TYPE, handle_param_type, ENFORCE_UNIQUE_TAG) \
    X(0x03, TAG_PARAM, handle_param, ENFORCE_UNIQUE_TAG)           \
    X(0x04, TAG_VISIBLE, handle_param_visible, ENFORCE_UNIQUE_TAG) \
    X(0x05, TAG_CONSTRAINT, handle_param_constraint, ALLOW_MULTIPLE_TAG)

// Generate TLV parser for Field
DEFINE_TLV_PARSER(FIELD_TAGS, NULL, field_tlv_parser)

static bool handle_version(const tlv_data_t *data, s_field_ctx *context) {
    return tlv_get_uint8(data, &context->field->version, 0, UINT8_MAX);
}

static bool handle_name(const tlv_data_t *data, s_field_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value.ptr,
                           data->value.size,
                           context->field->name,
                           sizeof(context->field->name));
    return true;
}

static bool handle_param_type(const tlv_data_t *data, s_field_ctx *context) {
    if (!tlv_get_uint8(data, &context->field->param_type, 0, UINT8_MAX)) {
        return false;
    }
    switch (context->field->param_type) {
        case PARAM_TYPE_RAW:
        case PARAM_TYPE_AMOUNT:
        case PARAM_TYPE_TOKEN_AMOUNT:
        case PARAM_TYPE_NFT:
        case PARAM_TYPE_DATETIME:
        case PARAM_TYPE_DURATION:
        case PARAM_TYPE_UNIT:
        case PARAM_TYPE_ENUM:
        case PARAM_TYPE_TRUSTED_NAME:
        case PARAM_TYPE_CALLDATA:
        case PARAM_TYPE_TOKEN:
        case PARAM_TYPE_NETWORK:
            break;
        default:
            PRINTF("Error: Unsupported param type (%u)\n", context->field->param_type);
            return false;
    }
    return true;
}

static bool handle_param_visible(const tlv_data_t *data, s_field_ctx *context) {
    e_param_visibility visibility = PARAM_VISIBILITY_ALWAYS;

    // Set visibility
    if (!tlv_get_uint8(data, (uint8_t *) &visibility, 0, PARAM_VISIBILITY_MAX - 1)) {
        return false;
    }
    // Set visibility
    context->field->visibility = visibility;
    return true;
}

static bool handle_param_constraint(const tlv_data_t *data, s_field_ctx *context) {
    // Check visibility presence
    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_VISIBLE)) {
        PRINTF("Error: No VISIBLE detected for the current FIELD struct!\n");
        return false;
    }
    // Check visibility type
    if (context->field->visibility == PARAM_VISIBILITY_ALWAYS) {
        PRINTF("Error: CONSTRAINT present but VISIBLE is not MUST_BE or IF_NOT_IN!\n");
        return false;
    }
    // Allocate new constraint node
    s_field_constraint *node = NULL;
    if (mem_buffer_allocate((void **) &node, sizeof(s_field_constraint)) == false) {
        PRINTF("Error: Failed to allocate memory for constraint node!\n");
        return false;
    }
    node->size = data->value.size;
    // Allocate value buffer
    if (mem_buffer_allocate((void **) &node->value, data->value.size) == false) {
        PRINTF("Error: Failed to allocate memory for constraint value!\n");
        app_mem_free((void **) &node);
        return false;
    }
    memcpy(node->value, data->value.ptr, data->value.size);
    // Add to linked list
    flist_push_back((flist_node_t **) &context->field->constraints, (flist_node_t *) node);
    return true;
}

static bool handle_param(const tlv_data_t *data, s_field_ctx *context) {
    u_param_context param_ctx = {0};
    bool ret;

    // Check PARAM_TYPE presence
    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_PARAM_TYPE)) {
        PRINTF("Error: Received PARAM without a previous PARAM_TYPE!\n");
        return false;
    }
    switch (context->field->param_type) {
        case PARAM_TYPE_RAW:
            param_ctx.raw_ctx.param = &context->field->param_raw;
            ret = handle_param_raw_struct(&data->value, &param_ctx.raw_ctx);
            break;
        case PARAM_TYPE_AMOUNT:
            param_ctx.amount_ctx.param = &context->field->param_amount;
            ret = handle_param_amount_struct(&data->value, &param_ctx.amount_ctx);
            break;
        case PARAM_TYPE_TOKEN_AMOUNT:
            param_ctx.token_amount_ctx.param = &context->field->param_token_amount;
            ret = handle_param_token_amount_struct(&data->value, &param_ctx.token_amount_ctx);
            break;
        case PARAM_TYPE_NFT:
            param_ctx.nft_ctx.param = &context->field->param_nft;
            ret = handle_param_nft_struct(&data->value, &param_ctx.nft_ctx);
            break;
        case PARAM_TYPE_DATETIME:
            param_ctx.datetime_ctx.param = &context->field->param_datetime;
            ret = handle_param_datetime_struct(&data->value, &param_ctx.datetime_ctx);
            break;
        case PARAM_TYPE_DURATION:
            param_ctx.duration_ctx.param = &context->field->param_duration;
            ret = handle_param_duration_struct(&data->value, &param_ctx.duration_ctx);
            break;
        case PARAM_TYPE_UNIT:
            param_ctx.unit_ctx.param = &context->field->param_unit;
            ret = handle_param_unit_struct(&data->value, &param_ctx.unit_ctx);
            break;
        case PARAM_TYPE_ENUM:
            param_ctx.enum_ctx.param = &context->field->param_enum;
            ret = handle_param_enum_struct(&data->value, &param_ctx.enum_ctx);
            break;
        case PARAM_TYPE_TRUSTED_NAME:
            param_ctx.trusted_name_ctx.param = &context->field->param_trusted_name;
            ret = handle_param_trusted_name_struct(&data->value, &param_ctx.trusted_name_ctx);
            break;
        case PARAM_TYPE_CALLDATA:
            param_ctx.calldata_ctx.param = &context->field->param_calldata;
            ret = handle_param_calldata_struct(&data->value, &param_ctx.calldata_ctx);
            break;
        case PARAM_TYPE_TOKEN:
            param_ctx.token_ctx.param = &context->field->param_token;
            ret = handle_param_token_struct(&data->value, &param_ctx.token_ctx);
            break;
        case PARAM_TYPE_NETWORK:
            param_ctx.network_ctx.param = &context->field->param_network;
            ret = handle_param_network_struct(&data->value, &param_ctx.network_ctx);
            break;
        default:
            return false;
    }
    return ret;
}

bool handle_field_struct(const buffer_t *buf, s_field_ctx *context) {
    return field_tlv_parser(buf, context, &context->received_tags);
}

bool verify_field_struct(const s_field_ctx *context) {
    // Check if struct version was provided
    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_VERSION)) {
        PRINTF("Error: no struct version specified!\n");
        return false;
    }

    // Verify required fields based on version
    switch (context->field->version) {
        case 1:
            if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                         TAG_VERSION,
                                         TAG_NAME,
                                         TAG_PARAM_TYPE,
                                         TAG_PARAM)) {
                PRINTF("Error: missing required field(s)\n");
                return false;
            }
            break;
        default:
            PRINTF("Error: unsupported field struct version (%u)\n", context->field->version);
            return false;
    }

    // Check optional visibility field
    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_VISIBLE)) {
        // Set default visibility if not provided
        context->field->visibility = PARAM_VISIBILITY_ALWAYS;
    }
    return true;
}

bool format_field(s_field *field) {
    bool ret;

    switch (field->param_type) {
        case PARAM_TYPE_RAW:
            ret = format_param_raw(field);
            break;
        case PARAM_TYPE_AMOUNT:
            ret = format_param_amount(&field->param_amount, field->name);
            break;
        case PARAM_TYPE_TOKEN_AMOUNT:
            ret = format_param_token_amount(&field->param_token_amount, field->name);
            break;
        case PARAM_TYPE_NFT:
            ret = format_param_nft(&field->param_nft, field->name);
            break;
        case PARAM_TYPE_DATETIME:
            ret = format_param_datetime(&field->param_datetime, field->name);
            break;
        case PARAM_TYPE_DURATION:
            ret = format_param_duration(&field->param_duration, field->name);
            break;
        case PARAM_TYPE_UNIT:
            ret = format_param_unit(&field->param_unit, field->name);
            break;
        case PARAM_TYPE_ENUM:
            ret = format_param_enum(&field->param_enum, field->name);
            break;
        case PARAM_TYPE_TRUSTED_NAME:
            ret = format_param_trusted_name(field);
            break;
        case PARAM_TYPE_CALLDATA:
            ret = format_param_calldata(&field->param_calldata, field->name);
            break;
        case PARAM_TYPE_TOKEN:
            ret = format_param_token(&field->param_token, field->name);
            break;
        case PARAM_TYPE_NETWORK:
            ret = format_param_network(&field->param_network, field->name);
            break;
        default:
            ret = false;
    }

    // Cleanup constraints after formatting (they are no longer needed)
    // This is safe to call even if constraints are NULL
    cleanup_field_constraints(field);

    // so that EIP-712 error-handling does trigger
    strings.tmp.tmp[0] = '\0';
    return ret;
}

static void constraint_node_del(flist_node_t *node) {
    if (node != NULL) {
        s_field_constraint *constraint = (s_field_constraint *) node;
        app_mem_free((void *) constraint->value);
        app_mem_free((void *) constraint);
    }
}

void cleanup_field_constraints(s_field *field) {
    if (field != NULL && field->constraints != NULL) {
        flist_clear((flist_node_t **) &field->constraints, constraint_node_del);
    }
}
