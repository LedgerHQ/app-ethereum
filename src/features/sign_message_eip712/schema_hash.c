#include "schema_hash.h"
#include "typed_data.h"
#include "format_hash_field_type.h"

// the SDK does not define a SHA-224 type, define it here so it's easier
// to understand in the code
typedef cx_sha256_t cx_sha224_t;

// Tracks whether the next comma-separated JSON element is the first one
typedef struct {
    cx_hash_t *hash_ctx;
    bool first;
} s_schema_hash_ctx;

static bool compute_schema_hash_struct_field(const s_struct_712_field *field_ptr, void *context) {
    s_schema_hash_ctx *ctx = context;

    if (!ctx->first) {
        if (cx_hash_update(ctx->hash_ctx, (uint8_t *) ",", 1) != CX_OK) {
            return false;
        }
    }
    ctx->first = false;

    if (cx_hash_update(ctx->hash_ctx, (uint8_t *) "{\"name\":\"", 9) != CX_OK) {
        return false;
    }
    if (cx_hash_update(ctx->hash_ctx,
                       (uint8_t *) field_ptr->key_name,
                       strlen(field_ptr->key_name)) != CX_OK) {
        return false;
    }
    if (cx_hash_update(ctx->hash_ctx, (uint8_t *) "\",\"type\":\"", 10) != CX_OK) {
        return false;
    }
    if (!format_hash_field_type(field_ptr, ctx->hash_ctx)) {
        return false;
    }
    return cx_hash_update(ctx->hash_ctx, (uint8_t *) "\"}", 2) == CX_OK;
}

static bool compute_schema_hash_struct(const s_struct_712 *struct_ptr, void *context) {
    s_schema_hash_ctx *ctx = context;
    s_schema_hash_ctx field_ctx = {.hash_ctx = ctx->hash_ctx, .first = true};

    if (!ctx->first) {
        if (cx_hash_update(ctx->hash_ctx, (uint8_t *) ",", 1) != CX_OK) {
            return false;
        }
    }
    ctx->first = false;

    if (cx_hash_update(ctx->hash_ctx, (uint8_t *) "\"", 1) != CX_OK) {
        return false;
    }
    if (cx_hash_update(ctx->hash_ctx, (uint8_t *) struct_ptr->name, strlen(struct_ptr->name)) !=
        CX_OK) {
        return false;
    }
    if (cx_hash_update(ctx->hash_ctx, (uint8_t *) "\":[", 3) != CX_OK) {
        return false;
    }
    if (!td_visit_struct_fields(struct_ptr, compute_schema_hash_struct_field, &field_ctx)) {
        return false;
    }
    return cx_hash_update(ctx->hash_ctx, (uint8_t *) "]", 1) == CX_OK;
}

/**
 * Compute the schema hash
 *
 * The schema hash is the value of the root field "types" in the JSON data,
 * stripped of all its spaces and newlines. This function reconstructs the JSON syntax
 * from the stored typed data.
 *
 * @return whether the schema hash was successful or not
 */
bool compute_schema_hash(uint8_t hash[CX_SHA224_SIZE]) {
    cx_sha224_t hash_ctx;
    s_schema_hash_ctx struct_ctx = {.hash_ctx = (cx_hash_t *) &hash_ctx, .first = true};

    if (cx_sha224_init_no_throw(&hash_ctx) != CX_OK) {
        return false;
    }

    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) "{", 1) != CX_OK) {
        return false;
    }
    if (!td_visit_structs(compute_schema_hash_struct, &struct_ctx)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) "}", 1) != CX_OK) {
        return false;
    }

    return cx_hash_final((cx_hash_t *) &hash_ctx, hash) == CX_OK;
}
