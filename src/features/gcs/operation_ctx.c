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
#include "os_utils.h"  // is_zeroes_buffer

// The calls, in execution order
static s_operation_ctx *g_operation_ctx_list = NULL;
// The typed message the calls were made by, kept apart from them
static s_operation_ctx *g_operation_ctx_message = NULL;
// Whether the message info announces descriptors, which are still to be checked
static bool g_message_has_fields = false;
// The operation being described: a call, or the message while none is
static s_operation_ctx *g_operation_ctx_current = NULL;
s_calldata *g_parked_calldata = NULL;
// The values a row of the review shows, by the operation they were read for
typedef struct {
    flist_node_t _list;
    uint32_t key;
    uint8_t ctx_id;
    uint8_t index;
    uint8_t as;  // OPERATION_SHOWN_* the row can stand for
} s_shown_value;

// Past it, a value is not recorded, and the review adds its row: a duplicate rather than a gap
#define MAX_SHOWN_VALUES 32

static s_shown_value *g_shown_values = NULL;
static uint8_t g_next_ctx_id = 0;

// to be used as a \ref f_list_node_del
static void delete_shown_value(s_shown_value *node) {
    APP_MEM_FREE(node);
}

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

uint8_t get_current_operation_batch_size(void) {
    if (g_operation_ctx_current == NULL) return 0;
    return g_operation_ctx_current->batch_size;
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

// Add the row of an address, by its name when it has one
static bool add_address_row(const char *key, const uint8_t *addr, uint64_t chain_id) {
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    e_param_type param_type;
    e_name_type types[] = {TN_TYPE_ACCOUNT};
    e_name_source sources[] = {TN_SOURCE_ENS, TN_SOURCE_LAB, TN_SOURCE_MAB};
    e_addr_name_source name_source;
    const void *extra_data = NULL;

    // Address Book > Trusted Name > raw hex; extra_data carries the matched record for the alias
    // detail view
    if (!get_address_display_name(addr,
                                  chain_id,
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
    return add_address_to_field_table(param_type, key, buf, extra_data, addr);
}

// Add the row of a native amount, in the currency of the chain the call is on
static bool add_amount_row(const uint8_t *amount, uint64_t chain_id) {
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    const char *ticker = get_displayable_ticker(&chain_id, g_chain_config, true);

    return amountToString(amount, INT256_LENGTH, WEI_TO_ETHER, ticker, buf, buf_size) &&
           add_to_field_table(PARAM_TYPE_AMOUNT, "Amount", buf, NULL);
}

static bool add_network_row(uint64_t chain_id) {
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);

    return get_network_as_string_from_chain_id(buf, buf_size, chain_id) &&
           add_to_field_table(PARAM_TYPE_RAW, "Network", buf, NULL);
}

static bool process_empty_call(const s_operation_ctx *operation_ctx) {
    // nothing tells where a call that takes no chain from its message goes
    if (!operation_ctx->has_chain_id) {
        PRINTF("Error: a call is on no chain!\n");
        return false;
    }
    if (operation_ctx->has_amount) {
        if (!set_intent_field("Send", operation_ctx->batch_size) ||
            !add_amount_row(operation_ctx->amount, operation_ctx->chain_id)) {
            return false;
        }
    } else {
        if (!set_intent_field("Empty transaction", operation_ctx->batch_size)) {
            return false;
        }
    }
    if (!add_address_row("To", operation_ctx->to, operation_ctx->chain_id)) {
        return false;
    }
    // A call on another chain than the root one is told apart by its network
    if (!is_on_root_chain(operation_ctx) && !add_network_row(operation_ctx->chain_id)) {
        return false;
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
    flist_clear((flist_node_t **) &g_shown_values, (f_list_node_del) &delete_shown_value);
    g_next_ctx_id = 0;
}

bool operation_ctx_set_last_batch_size(uint8_t batch_size) {
    s_operation_ctx *last = g_operation_ctx_list;

    if (last == NULL) return false;
    while (((flist_node_t *) last)->next != NULL) {
        last = (s_operation_ctx *) ((flist_node_t *) last)->next;
    }
    last->batch_size = batch_size;
    return true;
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
    node->id = g_next_ctx_id++;
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
    node->id = g_next_ctx_id++;
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
        // kept as copies: the parent can be dropped before this call is described
        node->has_parent = true;
        node->parent_id = parent->id;
        memcpy(node->parent_from, parent->from, sizeof(node->parent_from));
        memcpy(node->parent_to, parent->to, sizeof(node->parent_to));
        node->parent_has_to = parent->has_to;
        node->parent_chain_id = parent->chain_id;
        node->parent_has_chain_id = parent->has_chain_id;
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

void operation_ctx_set_shown(uint8_t what, uint8_t as, uint32_t key, uint8_t index) {
    s_shown_value *node;

    if (g_operation_ctx_current == NULL) {
        return;
    }
    g_operation_ctx_current->shown |= what & as;
    if ((flist_size((flist_node_t **) &g_shown_values) >= MAX_SHOWN_VALUES) ||
        !APP_MEM_CALLOC((void **) &node, sizeof(*node))) {
        return;
    }
    node->key = key;
    node->ctx_id = g_operation_ctx_current->id;
    node->index = index;
    node->as = as;
    flist_push_back((flist_node_t **) &g_shown_values, (flist_node_t *) node);
}

static bool is_root_shown(uint8_t what) {
    const s_operation_ctx *root = get_root();

    return (root != NULL) && ((root->shown & what) != 0);
}

bool operation_ctx_is_root_value_shown(void) {
    return is_root_shown(OPERATION_SHOWN_VALUE);
}

bool operation_ctx_is_root_from_shown(void) {
    return is_root_shown(OPERATION_SHOWN_FROM);
}

bool operation_ctx_set_last_origin(const s_call_origin *origin) {
    s_operation_ctx *last = g_operation_ctx_list;

    if (last == NULL) return false;
    while (((flist_node_t *) last)->next != NULL) {
        last = (s_operation_ctx *) ((flist_node_t *) last)->next;
    }
    last->origin = *origin;
    return true;
}

// Whether a row of the operation that made the call shows the value the call took
static bool is_shown_by_parent(const s_operation_ctx *call,
                               uint8_t what,
                               const s_call_origin_value *origin) {
    if (!call->has_parent || !origin->set) {
        return false;
    }
    for (const s_shown_value *node = g_shown_values; node != NULL;
         node = (const s_shown_value *) ((const flist_node_t *) node)->next) {
        if ((node->ctx_id == call->parent_id) && ((node->as & what) != 0) &&
            (node->key == origin->key) && (node->index == origin->index)) {
            return true;
        }
    }
    return false;
}

static bool is_parent_callee(const s_operation_ctx *call, const uint8_t *addr) {
    return call->parent_has_to && (memcmp(addr, call->parent_to, ADDRESS_LENGTH) == 0);
}

static bool needs_row(const s_operation_ctx *call,
                      uint8_t what,
                      const s_call_origin_value *origin) {
    return ((call->shown & what) == 0) && !is_shown_by_parent(call, what, origin);
}

// The row is part of the review of the call, whose fields are all in already
static bool add_call_row(bool (*add)(const s_operation_ctx *call), const s_operation_ctx *call) {
    field_table_extend_intent();
    return add(call);
}

static bool add_to_row(const s_operation_ctx *call) {
    return add_address_row("To", call->to, call->chain_id);
}

static bool add_call_amount_row(const s_operation_ctx *call) {
    return add_amount_row(call->amount, call->chain_id);
}

static bool add_call_network_row(const s_operation_ctx *call) {
    return add_network_row(call->chain_id);
}

static bool add_from_row(const s_operation_ctx *call) {
    return add_address_row("From", call->from, call->chain_id);
}

bool operation_ctx_add_unshown_rows(void) {
    const s_operation_ctx *call = g_operation_ctx_current;

    if (!operation_ctx_is_describing_call()) {
        return false;
    }
    // a call made by no other operation is a root, whose rows are the review's own
    if (!call->has_parent) {
        return true;
    }
    // A call to the contract the parent calls is to the contract already reviewed
    if (!is_parent_callee(call, call->to) &&
        needs_row(call, OPERATION_SHOWN_TO, &call->origin.to) && !add_call_row(&add_to_row, call)) {
        return false;
    }
    if (call->has_amount && !is_zeroes_buffer(call->amount, sizeof(call->amount)) &&
        needs_row(call, OPERATION_SHOWN_VALUE, &call->origin.amount) &&
        !add_call_row(&add_call_amount_row, call)) {
        return false;
    }
    if (call->has_chain_id &&
        (!call->parent_has_chain_id || (call->chain_id != call->parent_chain_id)) &&
        needs_row(call, OPERATION_SHOWN_CHAIN_ID, &call->origin.chain_id) &&
        !add_call_row(&add_call_network_row, call)) {
        return false;
    }
    // A call made by the contract the parent calls is that contract's own: its sender is not new
    if ((memcmp(call->from, call->parent_from, ADDRESS_LENGTH) != 0) &&
        !is_parent_callee(call, call->from) &&
        needs_row(call, OPERATION_SHOWN_FROM, &call->origin.from) &&
        !add_call_row(&add_from_row, call)) {
        return false;
    }
    return true;
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
