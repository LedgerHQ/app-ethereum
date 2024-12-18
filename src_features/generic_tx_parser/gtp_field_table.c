#ifdef HAVE_GENERIC_TX_PARSER

#include <string.h>
#include "os_print.h"
#include "gtp_field_table.h"
#include "mem.h"

// type (1) | key_length (1) | value_length (2) | key ($key_length) | value ($value_length)

typedef struct {
    const uint8_t *start;
    size_t size;
} s_field_table;

static s_field_table g_table;

void field_table_init(void) {
    explicit_bzero(&g_table, sizeof(g_table));
}

static const uint8_t *get_next_table_entry(const uint8_t *ptr, s_field_table_entry *entry) {
    uint8_t key_len;
    uint16_t value_len;

    if (ptr == NULL) {
        return NULL;
    }
    if (entry != NULL) {
        memcpy(&entry->type, ptr, sizeof(entry->type));
    }
    ptr += sizeof(entry->type);
    memcpy(&key_len, ptr, sizeof(key_len));
    ptr += sizeof(key_len);
    memcpy(&value_len, ptr, sizeof(value_len));
    ptr += sizeof(value_len);
    if (entry != NULL) {
        entry->key = (char *) ptr;
    }
    ptr += key_len;
    if (entry != NULL) {
        entry->value = (char *) ptr;
    }
    ptr += value_len;
    return ptr;
}

// after this function, field_table_init() will have to be called before using any other field_table
// function
void field_table_cleanup(void) {
    const uint8_t *ptr = g_table.start;

    if (ptr != NULL) {
        for (size_t i = 0; i < g_table.size; ++i) {
            ptr = get_next_table_entry(ptr, NULL);
        }
        mem_dealloc(ptr - g_table.start);
        g_table.start = NULL;
    }
}

bool add_to_field_table(e_param_type type, const char *key, const char *value) {
    int offset = 0;
    uint8_t *ptr;
    uint8_t key_len = strlen(key) + 1;
    uint16_t value_len = strlen(value) + 1;

    if ((key == NULL) || (value == NULL)) {
        PRINTF("Error: NULL key/value!\n");
        return false;
    }
    PRINTF(">>> \"%s\": \"%s\"\n", key, value);
    if ((ptr = mem_alloc(sizeof(type) + sizeof(uint8_t) + sizeof(uint16_t) + key_len +
                         value_len)) == NULL) {
        return false;
    }
    if ((g_table.start == NULL) && (g_table.size == 0)) {
        g_table.start = ptr;
    }
    memcpy(&ptr[offset], &type, sizeof(type));
    offset += sizeof(type);
    memcpy(&ptr[offset], &key_len, sizeof(key_len));
    offset += sizeof(key_len);
    memcpy(&ptr[offset], &value_len, sizeof(value_len));
    offset += sizeof(value_len);
    memcpy(&ptr[offset], key, key_len);
    offset += key_len;
    memcpy(&ptr[offset], value, value_len);
    g_table.size += 1;
    return true;
}

size_t field_table_size(void) {
    return g_table.size;
}

bool get_from_field_table(int index, s_field_table_entry *entry) {
    const uint8_t *ptr = g_table.start;

    if ((size_t) index >= g_table.size) {
        return false;
    }
    if (ptr == NULL) {
        return false;
    }
    for (int i = 0; i <= index; ++i) {
        ptr = get_next_table_entry(ptr, entry);
    }
    return true;
}

#endif  // HAVE_GENERIC_TX_PARSER
