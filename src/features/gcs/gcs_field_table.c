#include <string.h>
#include "os_print.h"
#include "gcs_field_table.h"
#include "app_mem_utils.h"
#include "lists.h"
#include "shared_context.h"  // appState
#ifdef HAVE_EIP712_V1
#include "eip712_v1_ui_logic.h"
#endif  // HAVE_EIP712_V1
#include "operation_ctx.h"
#include "token_info.h"
#include "nft_info.h"
#ifdef HAVE_ADDRESS_BOOK
#include "handle_contacts.h"
#endif

typedef struct {
    flist_node_t _list;
    s_field_table_entry field;
    bool owns_extra_data;  // extra_data is a copy freed with the node
} s_field_table_node;

static s_field_table_node *g_table = NULL;

bool field_table_init(void) {
    if (g_table != NULL) {
        field_table_cleanup();
        return false;
    }
    return true;
}

// to be used as a \ref f_list_node_del
static void delete_table_node(s_field_table_node *node) {
    APP_MEM_FREE(node->field.key);
    APP_MEM_FREE(node->field.value);
    if (node->owns_extra_data) {
        APP_MEM_FREE((void *) node->field.extra_data);
    }
    APP_MEM_FREE(node);
}

void field_table_cleanup(void) {
    flist_clear((flist_node_t **) &g_table, (f_list_node_del) &delete_table_node);
}

// Size of the extra data kept as a copy by the table, 0 when it is only referenced
static size_t owned_extra_data_size(e_param_type type) {
    switch (type) {
        case PARAM_TYPE_TOKEN:
        case PARAM_TYPE_TOKEN_AMOUNT:
            return sizeof(s_token_info);
        case PARAM_TYPE_NFT:
            return sizeof(s_nft_info);
#ifdef HAVE_ADDRESS_BOOK
        case PARAM_TYPE_ADDRESS_BOOK:
            return sizeof(s_ab_contact);
#endif
        default:
            return 0;
    }
}

bool add_to_field_table(e_param_type type,
                        const char *key,
                        const char *value,
                        const void *extra_data) {
    uint8_t key_len;
    uint16_t value_len;
    size_t copy_size;
    s_field_table_node *node;

    if ((key == NULL) || (value == NULL)) {
        PRINTF("Error: NULL key/value!\n");
        return false;
    }
    PRINTF(">>> \"%s\": \"%s\"\n", key, value);
#ifdef HAVE_EIP712_V1
    if (appState == APP_STATE_SIGNING_EIP712) {
        if ((type == PARAM_TYPE_INTENT) && (txContext.current_batch_size > 1)) {
            // Special handling for intent in EIP712 mode
            if (!ui_712_set_intent()) {
                return false;
            }
            PRINTF("[Intent] Start\n");
        }
        if (!ui_712_set_title(key, strlen(key))) {
            return false;
        }
        if (!ui_712_set_value(value, strlen(value))) {
            return false;
        }
        return true;
    }
#endif  // HAVE_EIP712_V1
    if (APP_MEM_CALLOC((void **) &node, sizeof(*node)) == false) {
        return false;
    }
    key_len = strlen(key) + 1;
    value_len = strlen(value) + 1;
    if ((node->field.key = APP_MEM_ALLOC(key_len)) == NULL) {
        APP_MEM_FREE(node);
        return false;
    }
    if ((node->field.value = APP_MEM_ALLOC(value_len)) == NULL) {
        APP_MEM_FREE(node->field.key);
        APP_MEM_FREE(node);
        return false;
    }
    if (type == PARAM_TYPE_INTENT) {
        // Special handling for intent
        node->field.start_intent = true;
        type = PARAM_TYPE_RAW;  // store as raw
        PRINTF("[Intent] Start\n");
    } else if (type == PARAM_TYPE_SEPARATOR) {
        // Special handling for separator
        node->field.is_separator = true;
        type = PARAM_TYPE_RAW;  // store as raw
    } else {
        node->field.end_intent = validate_instruction_hash();
        if (node->field.end_intent) {
            PRINTF("[Intent] End\n");
        }
    }

    node->field.type = type;
    memcpy(node->field.key, key, key_len);
    memcpy(node->field.value, value, value_len);
    node->field.extra_data = extra_data;
    copy_size = owned_extra_data_size(type);
    if ((copy_size != 0) && (extra_data != NULL)) {
        // Snapshot the asset / contact: a later metadata update must not change what is shown
        void *copy;
        if (APP_MEM_CALLOC(&copy, copy_size) == false) {
            delete_table_node(node);
            return false;
        }
        memcpy(copy, extra_data, copy_size);
#ifdef HAVE_ADDRESS_BOOK
        if (type == PARAM_TYPE_ADDRESS_BOOK) {
            ((s_ab_contact *) copy)->_list.next = NULL;
        }
#endif
        node->field.extra_data = copy;
        node->owns_extra_data = true;
    }

    flist_push_back((flist_node_t **) &g_table, (flist_node_t *) node);
    return true;
}

bool add_address_to_field_table(e_param_type type,
                                const char *key,
                                const char *value,
                                const void *extra_data,
                                const uint8_t *addr) {
    flist_node_t *node;

    if (!add_to_field_table(type, key, value, extra_data)) {
        return false;
    }
    // EIP-712 values are not kept in the table
    if (appState == APP_STATE_SIGNING_EIP712) {
        return true;
    }
    for (node = (flist_node_t *) g_table; node->next != NULL; node = node->next) {
    }
    memcpy(((s_field_table_node *) node)->field.addr, addr, ADDRESS_LENGTH);
    ((s_field_table_node *) node)->field.has_addr = true;
    return true;
}

bool set_intent_field(const char *value) {
    e_param_type type = PARAM_TYPE_INTENT;

    if (txContext.current_batch_size == 1) {
        // Only one transaction in the batch, no need to mark as intent
        type = PARAM_TYPE_RAW;
    }
    return add_to_field_table(type, "Transaction type", value, NULL);
}

size_t field_table_size(void) {
    return flist_size((flist_node_t **) &g_table);
}

const s_field_table_entry *get_from_field_table(size_t index) {
    const s_field_table_node *node = g_table;

    for (size_t i = 0; i < index; ++i) {
        if (node == NULL) return NULL;
        node = (s_field_table_node *) ((flist_node_t *) node)->next;
    }
    return &node->field;
}
