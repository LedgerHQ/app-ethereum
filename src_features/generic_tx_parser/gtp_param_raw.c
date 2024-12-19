#ifdef HAVE_GENERIC_TX_PARSER

#include <inttypes.h>
#include "os_print.h"
#include "gtp_param_raw.h"
#include "uint256.h"
#include "read.h"
#include "gtp_field_table.h"
#include "utils.h"
#include "shared_context.h"

enum {
    TAG_VERSION = 0x00,
    TAG_VALUE = 0x01,
};

static bool handle_version(const s_tlv_data *data, s_param_raw_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_value(const s_tlv_data *data, s_param_raw_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

bool handle_param_raw_struct(const s_tlv_data *data, s_param_raw_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_VALUE:
            ret = handle_value(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

bool format_uint(const s_value *def, const s_parsed_value *value, char *buf, size_t buf_size) {
    uint256_t value256;

    (void) def;
    convertUint256BE(value->ptr, value->length, &value256);
    return tostring256(&value256, 10, buf, buf_size);
}

bool format_int(const s_value *def, const s_parsed_value *value, char *buf, size_t buf_size) {
    uint8_t tmp[32];
    bool ret;
    union {
        uint256_t value256;
        uint128_t value128;
        int64_t value64;
        int32_t value32;
        int16_t value16;
        int8_t value8;
    } uv;

    buf_shrink_expand(value->ptr, value->length, tmp, def->type_size);
    switch (def->type_size * 8) {
        case 256:
            convertUint256BE(tmp, def->type_size, &uv.value256);
            ret = tostring256_signed(&uv.value256, 10, buf, buf_size);
            break;
        case 128:
            convertUint128BE(tmp, def->type_size, &uv.value128);
            ret = tostring128_signed(&uv.value128, 10, buf, buf_size);
            break;
        case 64:
            uv.value64 = read_u64_be(tmp, 0);
            ret = snprintf(buf, buf_size, "%" PRId64, uv.value64) > 0;
            break;
        case 32:
            uv.value32 = read_u32_be(tmp, 0);
            ret = snprintf(buf, buf_size, "%" PRId32, uv.value32) > 0;
            break;
        case 16:
            uv.value16 = read_u16_be(tmp, 0);
            ret = snprintf(buf, buf_size, "%u" PRId16, uv.value16) > 0;
            break;
        case 8:
            uv.value8 = value->ptr[0];
            ret = snprintf(buf, buf_size, "%u" PRId8, uv.value8) > 0;
            break;
        default:
            ret = false;
    }
    return ret;
}

static bool format_addr(const s_value *def,
                        const s_parsed_value *value,
                        char *buf,
                        size_t buf_size) {
    uint8_t tmp[ADDRESS_LENGTH];

    (void) def;
    buf_shrink_expand(value->ptr, value->length, tmp, sizeof(tmp));
    return getEthDisplayableAddress(tmp, buf, buf_size, chainConfig->chainId);
}

static bool format_bool(const s_value *def,
                        const s_parsed_value *value,
                        char *buf,
                        size_t buf_size) {
    uint8_t tmp;

    (void) def;
    buf_shrink_expand(value->ptr, value->length, &tmp, 1);
    snprintf(buf, buf_size, "%s", tmp ? "true" : "false");
    return true;
}

static bool format_bytes(const s_value *def,
                         const s_parsed_value *value,
                         char *buf,
                         size_t buf_size) {
    (void) def;
#pragma GCC diagnostic ignored "-Wformat"
    snprintf(buf, buf_size, "0x%.*h", value->length, value->ptr);
#pragma GCC diagnostic warning "-Wformat"
    if ((2 + (value->length * 2) + 1) > buf_size) {
        memmove(&buf[buf_size - 1 - 3], "...", 3);
    }
    return true;
}

static bool format_string(const s_value *def,
                          const s_parsed_value *value,
                          char *buf,
                          size_t buf_size) {
    (void) def;
    str_cpy_explicit_trunc((char *) value->ptr, value->length, buf, buf_size);
    return true;
}

bool format_param_raw(const s_param_raw *param, const char *name) {
    bool ret = false;
    s_parsed_value_collection collec;
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);

    if (!value_get(&param->value, &collec)) {
        return false;
    }
    for (int i = 0; i < collec.size; ++i) {
        switch (param->value.type_family) {
            case TF_UINT:
                ret = format_uint(&param->value, &collec.value[i], buf, buf_size);
                break;
            case TF_INT:
                ret = format_int(&param->value, &collec.value[i], buf, buf_size);
                break;
            case TF_ADDRESS:
                ret = format_addr(&param->value, &collec.value[i], buf, buf_size);
                break;
            case TF_BOOL:
                ret = format_bool(&param->value, &collec.value[i], buf, buf_size);
                break;
            case TF_BYTES:
                ret = format_bytes(&param->value, &collec.value[i], buf, buf_size);
                break;
            case TF_STRING:
                ret = format_string(&param->value, &collec.value[i], buf, buf_size);
                break;
            case TF_UFIXED:
            case TF_FIXED:
            default:
                ret = false;
        }
        if (ret) ret = add_to_field_table(PARAM_TYPE_RAW, name, buf);
        if (!ret) break;
    }
    return ret;
}

#endif  // HAVE_GENERIC_TX_PARSER
