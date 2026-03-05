/**
 * @file field_validation_mocks.c
 * @brief Minimal mocks for testing gtp_field.c constraint validation logic
 *
 * These tests only validate VISIBLE and CONSTRAINT tag handling.
 * However, gtp_field.c contains switch statements that reference all
 * handle_param_*_struct and format_param_* functions, so the linker
 * requires these symbols even though they're never called at runtime.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// ============================================================================
// Stubs required by linker (never called at runtime in validation tests)
// ============================================================================

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
