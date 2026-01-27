#include <string.h>
#include "gtp_field.h"
#include "utils.h"
#include "os_print.h"
#include "shared_context.h"  // strings
#include "mem.h"             // app_mem_free
#include "mem_utils.h"       // mem_buffer_cleanup
#include "list.h"            // flist_push_back

enum {
    BIT_VERSION = 0,
    BIT_NAME,
    BIT_PARAM_TYPE,
    BIT_PARAM,
    BIT_VISIBLE,
};

enum {
    TAG_VERSION = 0x00,
    TAG_NAME = 0x01,
    PARAM_TYPE = 0x02,
    PARAM = 0x03,
    VISIBLE = 0x04,
    CONSTRAINT = 0x05,
};

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

static bool handle_version(const s_tlv_data *data, s_field_ctx *context) {
    if (data->length != sizeof(context->field->version)) {
        return false;
    }
    context->field->version = data->value[0];
    context->set_flags |= SET_BIT(BIT_VERSION);
    return true;
}

static bool handle_name(const s_tlv_data *data, s_field_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value,
                           data->length,
                           context->field->name,
                           sizeof(context->field->name));
    context->set_flags |= SET_BIT(BIT_NAME);
    return true;
}

static bool handle_param_type(const s_tlv_data *data, s_field_ctx *context) {
    if (data->length != sizeof(context->field->param_type)) {
        return false;
    }
    if (context->set_flags & SET_BIT(BIT_PARAM_TYPE)) {
        PRINTF("Error: More than one PARAM_TYPE in a FIELD struct!\n");
        return false;
    }
    context->field->param_type = data->value[0];
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
    context->set_flags |= SET_BIT(BIT_PARAM_TYPE);
    return true;
}

static bool handle_param_visible(const s_tlv_data *data, s_field_ctx *context) {
    e_param_visibility visibility = PARAM_VISIBILITY_ALWAYS;

    // Check length
    if (data->length != sizeof(context->field->visibility)) {
        return false;
    }
    // Check duplicates
    if (context->set_flags & SET_BIT(BIT_VISIBLE)) {
        PRINTF("Error: More than one VISIBLE in a FIELD struct!\n");
        return false;
    }
    // Set visibility
    visibility = data->value[0];
    // Check visibility field validity
    if (visibility >= PARAM_VISIBILITY_MAX) {
        PRINTF("Error: Unsupported visibility (%u)\n", context->field->visibility);
        return false;
    }
    // Set visibility
    context->field->visibility = visibility;
    context->set_flags |= SET_BIT(BIT_VISIBLE);
    return true;
}

static bool handle_param_constraint(const s_tlv_data *data, s_field_ctx *context) {
    // Check visibility presence
    if (!(context->set_flags & SET_BIT(BIT_VISIBLE))) {
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
    node->size = data->length;
    // Allocate value buffer
    if (mem_buffer_allocate((void **) &node->value, data->length) == false) {
        PRINTF("Error: Failed to allocate memory for constraint value!\n");
        app_mem_free((void **) &node);
        return false;
    }
    memcpy(node->value, data->value, data->length);
    // Add to linked list
    flist_push_back((s_flist_node **) &context->field->constraints, (s_flist_node *) node);
    return true;
}

static bool handle_param(const s_tlv_data *data, s_field_ctx *context) {
    f_tlv_data_handler handler;
    u_param_context param_ctx = {0};

    if (context->set_flags & SET_BIT(BIT_PARAM)) {
        PRINTF("Error: More than one PARAM in a FIELD struct!\n");
        return false;
    }
    if (!(context->set_flags & SET_BIT(BIT_PARAM_TYPE))) {
        PRINTF("Error: Received PARAM without a previous PARAM_TYPE!\n");
        return false;
    }
    switch (context->field->param_type) {
        case PARAM_TYPE_RAW:
            handler = (f_tlv_data_handler) &handle_param_raw_struct;
            param_ctx.raw_ctx.param = &context->field->param_raw;
            break;
        case PARAM_TYPE_AMOUNT:
            handler = (f_tlv_data_handler) &handle_param_amount_struct;
            param_ctx.amount_ctx.param = &context->field->param_amount;
            break;
        case PARAM_TYPE_TOKEN_AMOUNT:
            handler = (f_tlv_data_handler) &handle_param_token_amount_struct;
            param_ctx.token_amount_ctx.param = &context->field->param_token_amount;
            break;
        case PARAM_TYPE_NFT:
            handler = (f_tlv_data_handler) &handle_param_nft_struct;
            param_ctx.nft_ctx.param = &context->field->param_nft;
            break;
        case PARAM_TYPE_DATETIME:
            handler = (f_tlv_data_handler) &handle_param_datetime_struct;
            param_ctx.datetime_ctx.param = &context->field->param_datetime;
            break;
        case PARAM_TYPE_DURATION:
            handler = (f_tlv_data_handler) &handle_param_duration_struct;
            param_ctx.duration_ctx.param = &context->field->param_duration;
            break;
        case PARAM_TYPE_UNIT:
            handler = (f_tlv_data_handler) &handle_param_unit_struct;
            param_ctx.unit_ctx.param = &context->field->param_unit;
            break;
        case PARAM_TYPE_ENUM:
            handler = (f_tlv_data_handler) &handle_param_enum_struct;
            param_ctx.enum_ctx.param = &context->field->param_enum;
            break;
        case PARAM_TYPE_TRUSTED_NAME:
            handler = (f_tlv_data_handler) &handle_param_trusted_name_struct;
            param_ctx.trusted_name_ctx.param = &context->field->param_trusted_name;
            break;
        case PARAM_TYPE_CALLDATA:
            handler = (f_tlv_data_handler) &handle_param_calldata_struct;
            param_ctx.calldata_ctx.param = &context->field->param_calldata;
            break;
        case PARAM_TYPE_TOKEN:
            handler = (f_tlv_data_handler) &handle_param_token_struct;
            param_ctx.token_ctx.param = &context->field->param_token;
            break;
        case PARAM_TYPE_NETWORK:
            handler = (f_tlv_data_handler) &handle_param_network_struct;
            param_ctx.network_ctx.param = &context->field->param_network;
            break;
        default:
            return false;
    }
    context->set_flags |= SET_BIT(BIT_PARAM);
    return tlv_parse(data->value, data->length, handler, &param_ctx);
}

bool handle_field_struct(const s_tlv_data *data, s_field_ctx *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_NAME:
            ret = handle_name(data, context);
            break;
        case PARAM_TYPE:
            ret = handle_param_type(data, context);
            break;
        case PARAM:
            ret = handle_param(data, context);
            break;
        case VISIBLE:
            ret = handle_param_visible(data, context);
            break;
        case CONSTRAINT:
            ret = handle_param_constraint(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

bool verify_field_struct(const s_field_ctx *context) {
    uint8_t required_bits = 0;

    // check if struct version was provided
    required_bits |= SET_BIT(BIT_VERSION);
    if ((context->set_flags & required_bits) != required_bits) {
        PRINTF("Error: no struct version specified!\n");
        return false;
    }

    // verify required fields
    switch (context->field->version) {
        case 1:
            required_bits |= SET_BIT(BIT_NAME) | SET_BIT(BIT_PARAM_TYPE) | SET_BIT(BIT_PARAM);
            break;
        default:
            PRINTF("Error: unsupported field struct version (%u)\n", context->field->version);
            return false;
    }

    if ((context->set_flags & required_bits) != required_bits) {
        PRINTF("Error: missing required field(s)\n");
        return false;
    }

    // check optional visibility field
    if (!(context->set_flags & SET_BIT(BIT_VISIBLE))) {
        // set default visibility if not provided
        context->field->visibility = PARAM_VISIBILITY_ALWAYS;
    }
    return true;
}

bool format_field(s_field *field) {
    bool ret;

    switch (field->param_type) {
        case PARAM_TYPE_RAW:
            ret = format_param_raw(&field->param_raw, field->name);
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
            ret = format_param_trusted_name(&field->param_trusted_name, field->name);
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

static void constraint_node_del(s_flist_node *node) {
    if (node != NULL) {
        s_field_constraint *constraint = (s_field_constraint *) node;
        app_mem_free((void *) constraint->value);
        app_mem_free((void *) constraint);
    }
}

void cleanup_field_constraints(s_field *field) {
    if (field != NULL && field->constraints != NULL) {
        flist_clear((s_flist_node **) &field->constraints, constraint_node_del);
    }
}
