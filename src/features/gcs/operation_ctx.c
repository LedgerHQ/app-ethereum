#include "operation_ctx.h"
#include "app_mem_utils.h"
#include "address_name_lookup.h"
#include "gcs_field_table.h"
#include "proxy_info.h"
#include "common_ui.h"       // ui_gcs_cleanup
#include "shared_context.h"  // appState
#include "utils.h"           // buf_shrink_expand
#include "get_public_key.h"
#include "network.h"
#include "hash_bytes.h"

// The calls, in execution order
static s_operation_ctx *g_operation_ctx_list = NULL;
// The typed message the calls were made by, kept apart from them
static s_operation_ctx *g_operation_ctx_message = NULL;
// Whether the message info announces descriptors, which are still to be checked
static bool g_message_has_fields = false;
// The operation being described: a call, or the message while none is
static s_operation_ctx *g_operation_ctx_current = NULL;
s_calldata *g_parked_calldata = NULL;
// Whether a descriptor field already displays the root transaction native value (CP_VALUE)
static bool g_root_value_shown = false;
// Whether a descriptor field already displays the root transaction sender (CP_FROM)
static bool g_root_from_shown = false;

static bool is_message(const s_operation_ctx *ctx) {
    return (ctx != NULL) && (ctx == g_operation_ctx_message);
}

// A call with no calldata to describe, which is shown from the call that made it
static bool is_empty_call(const s_operation_ctx *ctx) {
    return ctx->calldata == NULL;
}

// The operation the others come from: the typed message, else the transaction being signed
static s_operation_ctx *get_root(void) {
    return (g_operation_ctx_message != NULL) ? g_operation_ctx_message : g_operation_ctx_list;
}

bool operation_ctx_is_root(void) {
    return (g_operation_ctx_current != NULL) && (g_operation_ctx_current == get_root());
}

bool operation_ctx_is_describing_call(void) {
    return (g_operation_ctx_current != NULL) && !is_message(g_operation_ctx_current);
}

size_t get_operation_ctx_count(void) {
    return list_size((list_node_t **) &g_operation_ctx_list);
}

const s_operation_info *get_current_operation_info(void) {
    if (g_operation_ctx_current == NULL) return NULL;
    return g_operation_ctx_current->info;
}

bool get_current_operation_chain_id(uint64_t *chain_id) {
    // a message whose domain has no chain is on none
    if ((get_current_operation_info() == NULL) || !g_operation_ctx_current->has_chain_id) {
        return false;
    }
    *chain_id = g_operation_ctx_current->chain_id;
    return true;
}

const uint8_t *get_current_operation_from(void) {
    if (g_operation_ctx_current == NULL) return NULL;
    return g_operation_ctx_current->from;
}

const uint8_t *get_current_operation_to(void) {
    if ((g_operation_ctx_current == NULL) || !g_operation_ctx_current->has_to) return NULL;
    return g_operation_ctx_current->to;
}

const uint8_t *get_current_operation_amount(void) {
    // a typed message sends no native amount
    if ((g_operation_ctx_current == NULL) || is_message(g_operation_ctx_current)) return NULL;
    return g_operation_ctx_current->amount;
}

cx_hash_t *get_current_operation_fields_hash_ctx(void) {
    if (g_operation_ctx_current == NULL) return NULL;
    return (cx_hash_t *) &g_operation_ctx_current->fields_hash_ctx;
}

bool get_current_operation_key(s_operation_key *key) {
    const s_operation_info *info = get_current_operation_info();
    const uint8_t *callee;
    const uint8_t *selector;

    if (info == NULL) {
        return false;
    }
    memset(key, 0, sizeof(*key));
    key->context = info->context;
    switch (info->context) {
        case OPERATION_INFO_CTX_SIGN_TX:
            // the selector the calldata carries, which the info was checked against
            if (!get_current_operation_chain_id(&key->chain_id) ||
                ((callee = get_current_operation_to()) == NULL) ||
                ((selector = calldata_get_selector(get_current_calldata())) == NULL)) {
                return false;
            }
            memcpy(key->contract_addr, callee, sizeof(key->contract_addr));
            memcpy(key->selector, selector, sizeof(key->selector));
            return true;
        case OPERATION_INFO_CTX_EIP712:
            memcpy(key->domain_separator, info->domain_separator, sizeof(key->domain_separator));
            memcpy(key->primary_type_hash, info->primary_type_hash, sizeof(key->primary_type_hash));
            return true;
        default:
            return false;
    }
}

const s_operation_info *get_root_operation_info(void) {
    const s_operation_ctx *root = get_root();

    if (root == NULL) return NULL;
    return root->info;
}

s_calldata *get_current_calldata(void) {
    if (g_operation_ctx_current == NULL) return NULL;
    return g_operation_ctx_current->calldata;
}

s_calldata *get_root_calldata(void) {
    const s_operation_ctx *root = get_root();

    if (root == NULL) return NULL;
    return root->calldata;
}

/**
 * noinline: keeps the hash context copy out of the FIELD handler's frame, which
 * stays on the stack during the nested group recursion.
 */
__attribute__((noinline)) static bool validate_inst_hash_on(const s_operation_ctx *operation_ctx) {
    cx_sha3_t hash_ctx;
    uint8_t hash[sizeof(operation_ctx->info->fields_hash)];

    if ((operation_ctx == NULL) || (operation_ctx->info == NULL)) return false;
    // copy it locally, because the cx_hash call will modify it
    memcpy(&hash_ctx, &operation_ctx->fields_hash_ctx, sizeof(hash_ctx));
    if (finalize_hash((cx_hash_t *) &hash_ctx, hash, sizeof(hash)) != true) {
        return false;
    }
    return memcmp(operation_ctx->info->fields_hash, hash, sizeof(hash)) == 0;
}

bool validate_instruction_hash(void) {
    return validate_inst_hash_on(g_operation_ctx_current);
}

bool operation_ctx_has_message(void) {
    return g_operation_ctx_message != NULL;
}

bool operation_ctx_message_has_pending_fields(void) {
    return operation_ctx_has_message() && g_message_has_fields;
}

bool operation_ctx_message_fields_match(void) {
    return operation_ctx_message_has_pending_fields() &&
           validate_inst_hash_on(g_operation_ctx_message);
}

static void delete_operation_ctx(s_operation_ctx *node) {
    if (node->info != NULL) {
        delete_operation_info(node->info);
    }
    if (node->calldata != NULL) {
        if (g_parked_calldata == node->calldata) {
            g_parked_calldata = NULL;
        }
        calldata_delete(node->calldata);
    }
    APP_MEM_FREE(node);
}

/**
 * Drop the call being described, which hands the description back to what came before it
 *
 * @return whether a call was dropped, which it is not when none is being described
 */
bool operation_ctx_pop(void) {
    list_node_t *old_current = (list_node_t *) g_operation_ctx_current;

    // only a call is popped, which the message is not
    if (!operation_ctx_is_describing_call()) {
        return false;
    }
    // what was being described before the call is the previous call, else the message
    g_operation_ctx_current = (s_operation_ctx *) old_current->prev;
    if (g_operation_ctx_current == NULL) {
        g_operation_ctx_current = g_operation_ctx_message;
    }
    return list_remove((list_node_t **) &g_operation_ctx_list,
                       old_current,
                       (f_list_node_del) &delete_operation_ctx);
}

// Whether a call is on the chain of the root of the operations: the transaction's, or the message's
static bool is_on_root_chain(const s_operation_ctx *call) {
    const s_operation_ctx *root = get_root();

    // a message whose domain has no chain is on none
    return (root != NULL) && root->has_chain_id && call->has_chain_id &&
           (call->chain_id == root->chain_id);
}

static bool process_empty_call(const s_operation_ctx *operation_ctx) {
    const char *ticker;
    uint8_t decimals;
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    e_param_type param_type;

    // nothing tells where a call that takes no chain from its message goes
    if (!operation_ctx->has_chain_id) {
        PRINTF("Error: a call is on no chain!\n");
        return false;
    }
    if (operation_ctx->has_amount) {
        if (!set_intent_field("Send")) {
            return false;
        }
        // The nested context carries its own (possibly cross-chain) chain ID
        ticker = get_displayable_ticker(&operation_ctx->chain_id, g_chain_config, true);
        decimals = WEI_TO_ETHER;
        if (!amountToString(operation_ctx->amount,
                            sizeof(operation_ctx->amount),
                            decimals,
                            ticker,
                            buf,
                            buf_size)) {
            return false;
        }
        if (!add_to_field_table(PARAM_TYPE_AMOUNT, "Amount", buf, NULL)) {
            return false;
        }
    } else {
        if (!set_intent_field("Empty transaction")) {
            return false;
        }
    }

    e_name_type types[] = {TN_TYPE_ACCOUNT};
    e_name_source sources[] = {TN_SOURCE_ENS, TN_SOURCE_LAB, TN_SOURCE_MAB};
    e_addr_name_source name_source;
    const void *extra_data = NULL;

    // Address Book > Trusted Name > raw hex; extra_data carries the matched record for the alias
    // detail view
    if (!get_address_display_name(operation_ctx->to,
                                  operation_ctx->chain_id,
                                  ARRAYLEN(types),
                                  types,
                                  ARRAYLEN(sources),
                                  sources,
                                  buf,
                                  buf_size,
                                  &name_source,
                                  &extra_data)) {
        return false;
    }
    // param_type drives how the field is rendered: ADDRESS_BOOK and TRUSTED_NAME
    // entries carry an alias detail view (tappable name), RAW shows the plain address
    switch (name_source) {
        case ADDR_NAME_FROM_ADDRESS_BOOK:
            param_type = PARAM_TYPE_ADDRESS_BOOK;
            break;
        case ADDR_NAME_FROM_TRUSTED_NAME:
            param_type = PARAM_TYPE_TRUSTED_NAME;
            break;
        default:
            param_type = PARAM_TYPE_RAW;
            break;
    }
    if (!add_address_to_field_table(param_type, "To", buf, extra_data, operation_ctx->to)) {
        return false;
    }
    // A call on another chain than the root one is told apart by its network
    if (!is_on_root_chain(operation_ctx)) {
        if (!get_network_as_string_from_chain_id(buf, buf_size, operation_ctx->chain_id) ||
            !add_to_field_table(PARAM_TYPE_RAW, "Network", buf, NULL)) {
            return false;
        }
    }
    list_remove((list_node_t **) &g_operation_ctx_list,
                (list_node_t *) operation_ctx,
                (f_list_node_del) &delete_operation_ctx);
    return true;
}

bool process_empty_calls_before(void) {
    list_node_t *tmp = (list_node_t *) g_operation_ctx_current;
    // Rewind to the first of the contiguous empty predecessors: they are shown in execution order
    while ((tmp->prev != NULL) && is_empty_call((s_operation_ctx *) tmp->prev)) {
        tmp = tmp->prev;
    }
    while (tmp != (list_node_t *) g_operation_ctx_current) {
        // process_empty_call calls list_remove + delete_operation_ctx, which frees tmp.
        // Ensure reading tmp->next before the call to avoid use-after-free.
        list_node_t *next = (list_node_t *) ((flist_node_t *) tmp)->next;
        if (!process_empty_call((s_operation_ctx *) tmp)) {
            return false;
        }
        tmp = next;
    }
    return true;
}

bool process_empty_calls_after(void) {
    flist_node_t *tmp = ((flist_node_t *) g_operation_ctx_current)->next;
    while ((tmp != NULL) && is_empty_call((s_operation_ctx *) tmp)) {
        // process_empty_call calls list_remove + delete_operation_ctx, which frees tmp.
        // Ensure reading tmp->next before the call to avoid use-after-free.
        flist_node_t *next = tmp->next;
        if (!process_empty_call((s_operation_ctx *) tmp)) {
            return false;
        }
        tmp = next;
    }
    return true;
}

bool process_remaining_empty_calls(void) {
    s_operation_ctx *call;

    while ((call = g_operation_ctx_list) != NULL) {
        // a call with calldata is waiting for its description
        if (call->calldata != NULL) {
            return false;
        }
        // process_empty_call removes the context it processes
        if (!process_empty_call(call)) {
            return false;
        }
    }
    return true;
}

bool find_matching_operation_ctx(const uint8_t *contract_addr,
                                 const uint8_t *selector,
                                 const uint64_t *chain_id) {
    const uint8_t *proxy_implem;
    s_operation_ctx *expected = NULL;

    // Only the next undescribed context in execution order is eligible, so the
    // host cannot reorder the review vs the actual call sequence. Empty
    // transactions have no TX_INFO and are handled by process_empty_calls_*().
    for (s_operation_ctx *tmp = g_operation_ctx_list; tmp != NULL;
         tmp = (s_operation_ctx *) ((flist_node_t *) tmp)->next) {
        if ((tmp->info == NULL) && (tmp->calldata != NULL)) {
            expected = tmp;
            break;
        }
    }
    if (expected == NULL) {
        return false;
    }

    proxy_implem = get_implem_contract(chain_id, expected->to, selector);
    if ((memcmp((proxy_implem != NULL) ? proxy_implem : expected->to,
                contract_addr,
                ADDRESS_LENGTH) == 0) &&
        (memcmp(selector, expected->calldata->selector, CALLDATA_SELECTOR_SIZE) == 0) &&
        expected->has_chain_id && (*chain_id == expected->chain_id)) {
        g_operation_ctx_current = expected;
        return true;
    }
    return false;
}

void operation_ctx_cleanup(void) {
    list_clear((list_node_t **) &g_operation_ctx_list, (f_list_node_del) &delete_operation_ctx);
    if (g_operation_ctx_message != NULL) {
        delete_operation_ctx(g_operation_ctx_message);
        g_operation_ctx_message = NULL;
    }
    g_message_has_fields = false;
    g_operation_ctx_current = NULL;
    g_root_value_shown = false;
    g_root_from_shown = false;
}

bool operation_ctx_set_info(s_operation_info *info) {
    if (!operation_ctx_is_describing_call()) return false;
    g_operation_ctx_current->info = info;
    return true;
}

bool operation_ctx_init_message(s_operation_info *info,
                                const uint8_t from[ADDRESS_LENGTH],
                                const uint8_t contract[ADDRESS_LENGTH],
                                const uint64_t *chain_id) {
    s_operation_ctx *node;
    bool no_fields;

    // the message is the root of the operations, so it comes first, and there is only one
    if ((g_operation_ctx_message != NULL) || (g_operation_ctx_list != NULL) ||
        (APP_MEM_CALLOC((void **) &node, sizeof(*node)) == false)) {
        delete_operation_info(info);
        return false;
    }
    // the node owns the info from here on
    node->info = info;
    memcpy(node->from, from, sizeof(node->from));
    if (!operation_info_fields_hash_is_empty(info, &no_fields) ||
        (cx_sha3_init_no_throw(&node->fields_hash_ctx, 256) != CX_OK)) {
        PRINTF("Error: could not set up the message!\n");
        delete_operation_ctx(node);
        return false;
    }
    if (chain_id != NULL) {
        node->chain_id = *chain_id;
        node->has_chain_id = true;
    }
    if (contract != NULL) {
        memcpy(node->to, contract, sizeof(node->to));
        node->has_to = true;
    }
    g_message_has_fields = !no_fields;
    g_operation_ctx_message = node;
    g_operation_ctx_current = node;
    return true;
}

/**
 * Get the operation a call that is queued comes from, whose defaults it takes
 *
 * That is the operation being described when there is one, else the last one queued: the calls are
 * queued in the order they nest, before any matching has run.
 *
 * @return the operation, NULL if there is none
 */
static const s_operation_ctx *get_parent_of_new_call(void) {
    const s_operation_ctx *parent = g_operation_ctx_current;

    if (parent == NULL) {
        parent = g_operation_ctx_list;
        while ((parent != NULL) && (((const flist_node_t *) parent)->next != NULL)) {
            parent = (const s_operation_ctx *) ((const flist_node_t *) parent)->next;
        }
    }
    return (parent != NULL) ? parent : g_operation_ctx_message;
}

bool operation_ctx_init(s_calldata *calldata,
                        const uint8_t *from,
                        const uint8_t *to,
                        const uint8_t *amount,
                        const uint64_t *chain_id) {
    s_operation_ctx *node;

    if (APP_MEM_CALLOC((void **) &node, sizeof(*node)) == false) {
        return false;
    }
    node->calldata = calldata;
    node->has_to = true;
    if ((get_operation_ctx_count() == 0) && !operation_ctx_has_message()) {
        // a root call that names no sender is sent by the signer
        if ((from == NULL) && (get_public_key(node->from, ADDRESS_LENGTH) != SWO_SUCCESS)) {
            APP_MEM_FREE(node);
            return false;
        }
    } else {
        const s_operation_ctx *parent = get_parent_of_new_call();

        if (parent == NULL) {
            APP_MEM_FREE(node);
            return false;
        }
        memcpy(node->from, parent->from, sizeof(node->from));
        node->chain_id = parent->chain_id;
        node->has_chain_id = parent->has_chain_id;
    }

    if (from != NULL) {
        memcpy(node->from, from, sizeof(node->from));
    }
    if (to != NULL) {
        memcpy(node->to, to, sizeof(node->to));
    }
    if (amount != NULL) {
        memcpy(node->amount, amount, sizeof(node->amount));
        node->has_amount = true;
    }
    if (chain_id != NULL) {
        node->chain_id = *chain_id;
        node->has_chain_id = true;
    }

    if (cx_sha3_init_no_throw(&node->fields_hash_ctx, 256) != CX_OK) {
        APP_MEM_FREE(node);
        return false;
    }
    list_push_back((list_node_t **) &g_operation_ctx_list, (list_node_t *) node);

    // Ownership of the calldata has been transferred to the node.
    // Clear g_parked_calldata now so callers cannot double-free it if we return
    // false below (e.g. when field_table_init fails after the node is in the list
    // and will be freed by operation_ctx_cleanup via delete_operation_ctx).
    if (g_parked_calldata == calldata) {
        g_parked_calldata = NULL;
    }

    if ((appState == APP_STATE_SIGNING_TX) && (node == g_operation_ctx_list)) {
        return field_table_init();
    }
    return true;
}

void operation_ctx_set_root_value_shown(void) {
    g_root_value_shown = true;
}

bool operation_ctx_is_root_value_shown(void) {
    return g_root_value_shown;
}

void operation_ctx_set_root_from_shown(void) {
    g_root_from_shown = true;
}

bool operation_ctx_is_root_from_shown(void) {
    return g_root_from_shown;
}

void gcs_cleanup(void) {
    ui_gcs_cleanup();
    field_table_cleanup();
    operation_ctx_cleanup();
    // just in case
    if (g_parked_calldata != NULL) {
        calldata_delete(g_parked_calldata);
        g_parked_calldata = NULL;
    }
}
