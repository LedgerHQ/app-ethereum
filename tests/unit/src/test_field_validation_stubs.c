/**
 * @file test_field_validation_stubs.c
 * @brief Linker-filler stubs for test_field_validation.
 *
 * gtp_field.c switch statements reference all handle_param_* and
 * format_param_* functions; the linker requires these symbols even though
 * none are called during constraint-validation tests.
 *
 * Intentionally includes no param-type headers so the void * parameter
 * types avoid redeclaration conflicts with the real typed signatures.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

bool handle_param_raw_struct(void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool handle_param_amount_struct(void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool handle_param_token_amount_struct(void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool handle_param_nft_struct(void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool handle_param_datetime_struct(void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool handle_param_duration_struct(void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool handle_param_unit_struct(void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool handle_param_enum_struct(void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool handle_param_trusted_name_struct(void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool handle_param_calldata_struct(void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool handle_param_token_struct(void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool handle_param_network_struct(void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool format_param_raw(const void *field) {
    (void) field;
    return true;
}

bool format_param_amount(const void *param, const char *name) {
    (void) param;
    (void) name;
    return true;
}

bool format_param_token_amount(const void *param, const char *name) {
    (void) param;
    (void) name;
    return true;
}

bool format_param_nft(const void *param, const char *name) {
    (void) param;
    (void) name;
    return true;
}

bool format_param_datetime(const void *param, const char *name) {
    (void) param;
    (void) name;
    return true;
}

bool format_param_duration(const void *param, const char *name) {
    (void) param;
    (void) name;
    return true;
}

bool format_param_unit(const void *param, const char *name) {
    (void) param;
    (void) name;
    return true;
}

bool format_param_enum(const void *param, const char *name) {
    (void) param;
    (void) name;
    return true;
}

bool format_param_trusted_name(const void *field) {
    (void) field;
    return true;
}

bool format_param_calldata(const void *param, const char *name) {
    (void) param;
    (void) name;
    return true;
}

bool format_param_token(const void *param, const char *name) {
    (void) param;
    (void) name;
    return true;
}

bool format_param_network(const void *param, const char *name) {
    (void) param;
    (void) name;
    return true;
}

bool handle_param_group_struct(const void *buf, void *context) {
    (void) buf;
    (void) context;
    return true;
}

bool format_param_group(const void *field) {
    (void) field;
    return true;
}

void cleanup_param_group(void *group) {
    (void) group;
}
