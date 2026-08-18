#include "cmd_eip712_v2_sign.h"
#include "os_print.h"
#include "status_words.h"
#include "apdu_constants.h"
#include "shared_context.h"
#include "typed_data.h"
#include "common_ui.h"  // ui_712_v2_review, ui_error_blind_signing

#ifdef HAVE_PRINTF
// Debug-only trace of the value trees, compiled out of release builds

// One entry per open container, letting the flat visitor rebuild nesting
typedef struct {
    uint16_t remaining[TD_MAX_DEPTH];  // values left to visit at each open level
    char closer[TD_MAX_DEPTH];         // character closing each open level
    uint8_t depth;
} s_dump_ctx;

/**
 * Trace one value tree node
 *
 * The traversal is a pre-order walk carrying no depth, so nesting is rebuilt here by counting
 * down each container's values: a level closes once its last value has been visited.
 *
 * to be used as a \ref td_f_value_visitor
 */
static bool dump_node(const s_struct_712_value *node, void *context) {
    s_dump_ctx *ctx = context;
    const char *indent = "  ";

    for (int i = 0; i < ctx->depth; ++i) PRINTF("%s", indent);
    switch (node->kind) {
        case VAL_ATOMIC:
            PRINTF("%s = %.*h\n",
                   (node->field != NULL) ? node->field->key_name : "?",
                   node->length,
                   node->data);
            break;
        case VAL_STRUCT:
            PRINTF("%s {\n", (node->struct_type != NULL) ? node->struct_type->name : "?");
            break;
        case VAL_ARRAY:
            PRINTF("%s [\n", (node->field != NULL) ? node->field->key_name : "?");
            break;
    }

    if (ctx->depth > 0) {
        ctx->remaining[ctx->depth - 1] -= 1;
    }
    if (node->kind != VAL_ATOMIC) {
        if (ctx->depth >= TD_MAX_DEPTH) {
            PRINTF("EIP712 v2: value tree deeper than %d\n", TD_MAX_DEPTH);
            return false;
        }
        ctx->remaining[ctx->depth] = td_value_child_count(node);
        ctx->closer[ctx->depth] = (node->kind == VAL_STRUCT) ? '}' : ']';
        ctx->depth += 1;
    }
    // close every level this value completed, innermost first
    while ((ctx->depth > 0) && (ctx->remaining[ctx->depth - 1] == 0)) {
        ctx->depth -= 1;
        for (int i = 0; i < ctx->depth; ++i) PRINTF("%s", indent);
        PRINTF("%c\n", ctx->closer[ctx->depth]);
    }
    return true;
}

/**
 * Trace a whole value tree
 *
 * @param[in] traverse typed data traversal function selecting the tree
 */
static void dump_tree(bool (*traverse)(td_f_value_visitor, void *)) {
    s_dump_ctx ctx = {0};

    traverse(dump_node, &ctx);
}
#endif  // HAVE_PRINTF

uint16_t handle_eip712_v2_sign(uint8_t p1, uint8_t p2, uint8_t lc) {
    if ((p1 != 0) || (p2 != P2_EIP712_V2_IMPLEM)) {
        return SWO_INCORRECT_P1_P2;
    }
    // the derivation path came with the value tree, so this command carries no data
    if (lc != 0) {
        return SWO_INCORRECT_DATA;
    }
    if (appState != APP_STATE_PREPARING_EIP712) {
        return SWO_COMMAND_NOT_ALLOWED;
    }
    if (!td_has_domain() || !td_has_message()) {
        PRINTF("EIP712 v2: no message to sign\n");
        return SWO_COMMAND_NOT_ALLOWED;
    }
    if (!td_hash_pass()) {
        PRINTF("EIP712 v2: hash pass failed\n");
        return SWO_INCORRECT_DATA;
    }
#ifdef HAVE_PRINTF
    dump_tree(td_traverse_domain);
    dump_tree(td_traverse_message);
#endif
    // every value is shown raw, which is blind signing whatever the protocol version
    if (!N_storage.dataAllowed && !N_storage.verbose_eip712) {
        ui_error_blind_signing();
        ui_hold_error_screen();
        return SWO_INCORRECT_DATA;
    }
    if (!ui_712_v2_review()) {
        PRINTF("EIP712 v2: could not build the review\n");
        return SWO_INCORRECT_DATA;
    }
    return SWO_NO_RESPONSE;
}
