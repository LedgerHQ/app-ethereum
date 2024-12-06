#ifdef HAVE_GENERIC_TX_PARSER

#include <string.h>
#include "gtp_field_table.h"
#include "mem.h"

// type (1) | key_length (1) | value_length (2) | key ($key_length) | value ($value_length)

typedef struct {
    const uint8_t *start;
    size_t size;
} s_field_table;

static s_field_table table;

void field_table_init(void) {
    explicit_bzero(&table, sizeof(table));
}

void field_table_cleanup(void) {
    const uint8_t *ptr = table.start;
    uint8_t key_len;
    uint16_t value_len;

    for (size_t i = 0; i < table.size; ++i) {
        ptr += sizeof(e_param_type);
        memcpy(&key_len, ptr, sizeof(key_len));
        ptr += sizeof(key_len);
        memcpy(&value_len, ptr, sizeof(value_len));
        ptr += sizeof(value_len);
        ptr += key_len;
        ptr += value_len;
    }
    mem_dealloc(ptr - table.start);
}

bool add_to_field_table(uint8_t type, const char *key, const char *value) {
    int offset = 0;
    uint8_t *ptr;
    uint8_t key_len = strlen(key) + 1;
    uint16_t value_len = strlen(value) + 1;

    PRINTF(">>> \"%s\": \"%s\"\n", key, value);
    if ((ptr = mem_alloc(sizeof(type) + sizeof(uint8_t) + sizeof(uint16_t) + key_len + value_len)) == NULL) {
        return false;
    }
    if ((table.start == NULL) && (table.size == 0)) {
        table.start = ptr;
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
    offset += value_len;
    table.size += 1;
    return true;
}

size_t field_table_size(void) {
    return table.size;
}

bool get_from_field_table(int index, s_field_table_entry *entry) {
    const uint8_t *ptr = table.start;
    uint8_t key_len;
    uint16_t value_len;

    if ((size_t)index >= table.size) {
        return false;
    }
    if (ptr == NULL) {
        return false;
    }
    for (int i = 0; i <= index; ++i) {
        memcpy(&entry->type, ptr, sizeof(entry->type));
        ptr += sizeof(entry->type);
        memcpy(&key_len, ptr, sizeof(key_len));
        ptr += sizeof(key_len);
        memcpy(&value_len, ptr, sizeof(value_len));
        ptr += sizeof(value_len);
        entry->key = (char*)ptr;
        ptr += key_len;
        entry->value = (char*)ptr;
        ptr += value_len;
    }
    return true;
}

#endif // HAVE_GENERIC_TX_PARSER
