#include <string.h>
#include "os_print.h"
#include "gtp_field_table.h"
#include "mem.h"
#include "list.h"
#include "shared_context.h"  // appState
#include "ui_logic.h"
#include "tx_ctx.h"

typedef struct {
    s_flist_node _list;
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
    if (node->field.key != NULL) app_mem_free(node->field.key);
    if (node->field.value != NULL) app_mem_free(node->field.value);
    app_mem_free(node);
}

void field_table_cleanup(void) {
    flist_clear((s_flist_node **) &g_table, (f_list_node_del) &delete_table_node);
}

bool add_to_field_table(e_param_type type, const char *key, const char *value) {
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
            PRINTF(">>> [Intent] Start\n");
        }
        ui_712_set_title(key, strlen(key));
        ui_712_set_value(value, strlen(value));
        return true;
    }
    if ((node = app_mem_alloc(sizeof(*node))) == NULL) {
        return false;
    }
    explicit_bzero(node, sizeof(*node));
    key_len = strlen(key) + 1;
    value_len = strlen(value) + 1;
    if ((node->field.key = app_mem_alloc(key_len)) == NULL) {
        app_mem_free(node);
        return false;
    }
    if ((node->field.value = app_mem_alloc(value_len)) == NULL) {
        app_mem_free(node->field.key);
        app_mem_free(node);
        return false;
    }
    if (type == PARAM_TYPE_INTENT) {
        // Special handling for intent
        node->field.start_intent = true;
        type = PARAM_TYPE_RAW;  // store as raw
        PRINTF(">>> [Intent] Start\n");
    } else {
        node->field.end_intent = validate_instruction_hash();
        if (node->field.end_intent) {
            PRINTF(">>> [Intent] End\n");
        }
    }

    node->field.type = type;
    memcpy(node->field.key, key, key_len);
    memcpy(node->field.value, value, value_len);

    flist_push_back((s_flist_node **) &g_table, (s_flist_node *) node);
    return true;
}

bool set_intent_field(const char *value) {
    e_param_type type = PARAM_TYPE_INTENT;

    if (txContext.current_batch_size == 1) {
        // Only one transaction in the batch, no need to mark as intent
        type = PARAM_TYPE_RAW;
    }
    return add_to_field_table(type, "Transaction type", value);
}

size_t field_table_size(void) {
    return flist_size((s_flist_node **) &g_table);
}

const s_field_table_entry *get_from_field_table(int index) {
    const s_field_table_node *node = g_table;

    for (int i = 0; i < index; ++i) {
        if (node == NULL) return NULL;
        node = (s_field_table_node *) ((s_flist_node *) node)->next;
    }
    return &node->field;
}
