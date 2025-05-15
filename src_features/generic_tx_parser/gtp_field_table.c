#include <string.h>
#include "os_print.h"
#include "gtp_field_table.h"
#include "mem.h"

typedef struct field_table_node {
    s_field_table_entry field;
    struct field_table_node *next;
} s_field_table_node;

typedef struct {
    s_field_table_node *nodes;
    size_t size;
} s_field_table;

static s_field_table g_table;

void field_table_init(void) {
    explicit_bzero(&g_table, sizeof(g_table));
}

// after this function, field_table_init() will have to be called before using any other field_table
// function
void field_table_cleanup(void) {
    s_field_table_node *node;
    s_field_table_node *next;

    node = g_table.nodes;
    while (node != NULL) {
        next = node->next;
        if (node->field.key != NULL) app_mem_free(node->field.key);
        if (node->field.value != NULL) app_mem_free(node->field.value);
        app_mem_free(node);
        node = next;
    }
    g_table.nodes = NULL;
}

bool add_to_field_table(e_param_type type, const char *key, const char *value) {
    uint8_t key_len;
    uint16_t value_len;
    s_field_table_node *node;
    s_field_table_node *tmp;

    if ((key == NULL) || (value == NULL)) {
        PRINTF("Error: NULL key/value!\n");
        return false;
    }
    if ((node = app_mem_alloc(sizeof(*node))) == NULL) {
        return false;
    }
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
    PRINTF(">>> \"%s\": \"%s\"\n", key, value);

    node->field.type = type;
    memcpy(node->field.key, key, key_len);
    memcpy(node->field.value, value, value_len);
    node->next = NULL;

    if (g_table.nodes == NULL) {
        g_table.nodes = node;
    } else {
        for (tmp = g_table.nodes; tmp->next != NULL; tmp = tmp->next)
            ;
        tmp->next = node;
    }

    g_table.size += 1;
    return true;
}

size_t field_table_size(void) {
    return g_table.size;
}

const s_field_table_entry *get_from_field_table(int index) {
    const s_field_table_node *node = g_table.nodes;

    if ((size_t) index >= g_table.size) {
        return NULL;
    }
    for (int i = 0; i < index; ++i) {
        if (node == NULL) return NULL;
        node = node->next;
    }
    return &node->field;
}
