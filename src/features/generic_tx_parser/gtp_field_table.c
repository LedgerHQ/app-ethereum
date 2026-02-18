#include <string.h>
#include "os_print.h"
#include "gtp_field_table.h"
#include "app_mem_utils.h"
#include "lists.h"
#include "shared_context.h"  // appState
#include "ui_logic.h"
#include "tx_ctx.h"

typedef struct {
    flist_node_t _list;
    s_field_table_entry field;
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
    APP_MEM_FREE(node);
}

void field_table_cleanup(void) {
    flist_clear((flist_node_t **) &g_table, (f_list_node_del) &delete_table_node);
}

bool add_to_field_table(e_param_type type,
                        const char *key,
                        const char *value,
                        const void *extra_data) {
    uint8_t key_len;
    uint16_t value_len;
    s_field_table_node *node;

    if ((key == NULL) || (value == NULL)) {
        PRINTF("Error: NULL key/value!\n");
        return false;
    }
    PRINTF(">>> \"%s\": \"%s\"\n", key, value);
    if (appState == APP_STATE_SIGNING_EIP712) {
        if ((type == PARAM_TYPE_INTENT) && (txContext.current_batch_size > 1)) {
            // Special handling for intent in EIP712 mode
            ui_712_set_intent();
            PRINTF("[Intent] Start\n");
        }
        ui_712_set_title(key, strlen(key));
        ui_712_set_value(value, strlen(value));
        return true;
    }
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

    flist_push_back((flist_node_t **) &g_table, (flist_node_t *) node);
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

const s_field_table_entry *get_from_field_table(int index) {
    const s_field_table_node *node = g_table;

    for (int i = 0; i < index; ++i) {
        if (node == NULL) return NULL;
        node = (s_field_table_node *) ((flist_node_t *) node)->next;
    }
    return &node->field;
}
