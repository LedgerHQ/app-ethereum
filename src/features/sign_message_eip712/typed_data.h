#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lists.h"
#include "common_utils.h"

// Whether a declared array dimension is dynamically sized or has a fixed size
typedef enum { ARRAY_DYNAMIC = 0, ARRAY_FIXED_SIZE, ARRAY_TYPES_COUNT } e_array_type;

typedef enum {
    // contract defined struct
    TYPE_STRUCT = 0,
    // native types
    TYPE_SOL_INT,
    TYPE_SOL_UINT,
    TYPE_SOL_ADDRESS,
    TYPE_SOL_BOOL,
    TYPE_SOL_STRING,
    TYPE_SOL_BYTES_FIX,
    TYPE_SOL_BYTES_DYN,
    TYPES_COUNT
} e_type;

// One dimension of a (possibly multi-dimensional) array field declaration
typedef struct {
    e_array_type type;
    uint8_t size;
} s_struct_712_field_array_level;

// A single field declaration within a struct definition (schema, not value)
typedef struct {
    flist_node_t _list;
    union {
        char *struct_name;  // TYPE_STRUCT
        uint8_t type_size;  // native types
    };
    char *key_name;
    s_struct_712_field_array_level *array_levels;
    uint8_t array_level_count;
    e_type type;
} s_struct_712_field;

// A struct type declaration (schema, not value)
typedef struct {
    flist_node_t _list;
    char *name;
    s_struct_712_field *fields;
} s_struct_712;

// Kind of node found in a value tree (as opposed to a schema/field definition)
typedef enum { VAL_ATOMIC, VAL_STRUCT, VAL_ARRAY } e_val_kind;

#define IS_DYN(type) (((type) == TYPE_SOL_STRING) || ((type) == TYPE_SOL_BYTES_DYN))

// Which top-level value tree (if any) a node belongs to
typedef enum { ROOT_NONE = 0, ROOT_DOMAIN, ROOT_MESSAGE } e_root_type;

// A node in a message/domain value tree: either an atomic leaf, a struct, or an array
typedef struct struct_712_value {
    flist_node_t _list;
    union {
        const s_struct_712_field *field;  // VAL_ATOMIC, VAL_ARRAY
        const s_struct_712 *struct_type;  // VAL_STRUCT
    };
    union {
        uint8_t *data;                      // VAL_ATOMIC
        struct struct_712_value *children;  // VAL_STRUCT, VAL_ARRAY
    };
    uint16_t length;  // VAL_ATOMIC only; unused (0) for VAL_STRUCT/VAL_ARRAY
    e_val_kind kind;
} s_struct_712_value;

typedef struct {
    s_struct_712_value *domain;   // VAL_STRUCT → EIP712Domain
    s_struct_712_value *message;  // VAL_STRUCT → primaryType
} s_eip712_impl;

// Maximum nesting depth supported when traversing/building a value tree
#define TD_MAX_DEPTH 16

/**
 * Create a new, empty struct type definition
 *
 * @param[in] name struct type name
 * @return pointer to the new struct definition, or NULL on failure
 */
s_struct_712 *td_create_struct_def(const char *name);

/**
 * Register a struct definition as usable for future field/value declarations
 *
 * @param[in] struct_def struct definition to register
 * @return whether the registration succeeded
 */
bool td_add_struct_def(const s_struct_712 *struct_def);

/**
 * Append a field definition to a struct definition
 *
 * @param[in,out] struct_def struct definition to append to
 * @param[in] field_def field definition to append
 * @return whether the field was appended
 */
bool td_add_struct_field_def(s_struct_712 *struct_def, const s_struct_712_field *field_def);

/**
 * Discard a field definition created via td_create_field_def() that was never
 * attached to a struct (e.g. abandoned mid-assembly). The field and its
 * owned contents (struct/key name, array levels) are fully destroyed.
 *
 * @param[in] field field definition to discard; no-op if NULL
 */
void td_discard_struct_field(s_struct_712_field *field);

/**
 * Create a new atomic (leaf) value node with room for its raw value
 *
 * @param[in] field field definition this value corresponds to
 * @param[in] total_size size in bytes of the value's raw data
 * @return pointer to the new leaf node, or NULL on failure
 */
s_struct_712_value *td_create_leaf(const s_struct_712_field *field, size_t total_size);

/**
 * Discard a leaf created via td_create_leaf() that was never attached to the
 * tree (e.g. a field write abandoned mid-assembly). The leaf and its raw
 * value are fully destroyed.
 *
 * @param[in] leaf leaf to discard; no-op if NULL
 */
void td_discard_leaf(s_struct_712_value *leaf);

/**
 * Create a new struct value node (VAL_STRUCT), without children
 *
 * @param[in] struct_type struct definition this value is an instance of
 * @return pointer to the new struct node, or NULL on failure
 */
s_struct_712_value *td_create_struct(const s_struct_712 *struct_type);

/**
 * Create a new array value node (VAL_ARRAY), without children
 *
 * @param[in] field field definition this array corresponds to
 * @return pointer to the new array node, or NULL on failure
 */
s_struct_712_value *td_create_array(const s_struct_712_field *field);

/**
 * Discard a struct or array value node (created via td_create_struct() or
 * td_create_array()) that was never attached to the tree (e.g. abandoned
 * mid-assembly). Any children already attached to it are destroyed too,
 * recursively.
 *
 * @param[in] node struct or array value node to discard; no-op if NULL
 */
void td_discard_container_value(s_struct_712_value *node);

/**
 * Attach a child node to a struct or array value node
 *
 * @param[in,out] parent VAL_STRUCT/VAL_ARRAY node to attach the child to
 * @param[in] child node to attach
 * @return whether the child was attached
 */
bool td_append_child(s_struct_712_value *parent, const s_struct_712_value *child);

/**
 * Write raw bytes into a leaf's value, one chunk at a time
 *
 * @param[in,out] leaf VAL_ATOMIC node to write into
 * @param[in] offset offset in bytes into the leaf's value
 * @param[in] data bytes to copy
 * @param[in] length number of bytes to copy
 * @return whether the write succeeded (fails if it would exceed the leaf's declared size)
 */
bool td_leaf_write(s_struct_712_value *leaf, size_t offset, const uint8_t *data, size_t length);

/**
 * Check whether a leaf has received all of its expected bytes
 *
 * @param[in] leaf VAL_ATOMIC node to check
 * @param[in] filled number of bytes written so far
 * @return whether filled equals the leaf's declared total size
 */
bool td_leaf_is_complete(const s_struct_712_value *leaf, size_t filled);

/**
 * Create a new, empty field definition
 *
 * @param[in] type field's Solidity type
 * @return pointer to the new field definition, or NULL on failure
 */
s_struct_712_field *td_create_field_def(e_type type);

/**
 * Set the byte size of a native (non-struct) field type
 *
 * @param[in,out] field field definition to update
 * @param[in] type_size size in bytes
 * @return whether the size was set (fails for TYPE_STRUCT fields)
 */
bool td_field_set_type_size(s_struct_712_field *field, size_t type_size);

/**
 * Set the struct type name of a TYPE_STRUCT field
 *
 * @param[in,out] field field definition to update
 * @param[in] name struct type name (not required to be NUL-terminated)
 * @param[in] length length of name in bytes
 * @return whether the name was set (fails for non-struct fields)
 */
bool td_field_set_struct_name(s_struct_712_field *field, const uint8_t *name, size_t length);

/**
 * Set the number of array dimensions a field has, resetting any previously set levels
 *
 * @param[in,out] field field definition to update
 * @param[in] count number of array dimensions
 * @return whether the count was set
 */
bool td_field_set_array_level_count(s_struct_712_field *field, uint8_t count);

/**
 * Set one array dimension of a field previously sized with td_field_set_array_level_count()
 *
 * @param[in,out] field field definition to update
 * @param[in] index dimension index
 * @param[in] type ARRAY_DYNAMIC or ARRAY_FIXED_SIZE
 * @param[in] size fixed size, used only when type is ARRAY_FIXED_SIZE
 * @return whether the level was set
 */
bool td_field_set_array_level(s_struct_712_field *field,
                              uint8_t index,
                              e_array_type type,
                              uint8_t size);

/**
 * Set the key name of a field
 *
 * @param[in,out] field field definition to update
 * @param[in] name key name (not required to be NUL-terminated)
 * @param[in] length length of name in bytes
 * @return whether the name was set
 */
bool td_field_set_key_name(s_struct_712_field *field, const uint8_t *name, size_t length);

/**
 * Get type name from a struct field
 *
 * @param[in] ptr struct field pointer
 * @return type name pointer
 */
const char *td_get_struct_field_typename(const s_struct_712_field *ptr);

/**
 * Find struct with a given name
 *
 * @param[in] name_ptr struct name
 * @return pointer to struct, or NULL if not found
 */
const s_struct_712 *td_find_struct(const char *name_ptr);

/**
 * Set the message's EIP712Domain root value
 *
 * Takes ownership of @p node unconditionally: on success it becomes the domain
 * root; on failure it is destroyed. The caller must not use @p node again
 * after this call.
 *
 * @param[in] node VAL_STRUCT node whose type must be EIP712Domain
 * @return whether the domain was set (fails if already set or wrong type)
 */
bool td_set_domain(s_struct_712_value *node);

/**
 * Set the message's primaryType root value
 *
 * Takes ownership of @p node unconditionally: on success it becomes the message
 * root; on failure it is destroyed. The caller must not use @p node again
 * after this call.
 *
 * @param[in] node VAL_STRUCT node
 * @return whether the message was set (fails if already set or wrong kind)
 */
bool td_set_message(s_struct_712_value *node);

/**
 * @return whether the domain root value has been set
 */
bool td_has_domain(void);

/**
 * @return whether the message root value has been set
 */
bool td_has_message(void);

/**
 * Initialize the typed data context
 *
 * @return whether the initialization was successful
 */
bool td_init(void);

/**
 * Destroy all struct definitions and value trees, resetting the typed data context to empty
 */
void td_deinit(void);

/**
 * Compute the EIP-712 message hash over the current domain/message value trees
 *
 * @return whether the hash was computed successfully
 */
bool td_hash_pass(void);

/**
 * Fill @p chain_id with the chainId from the domain value tree.
 *
 * @return true if the field was found and copied into @p chain_id, false otherwise.
 */
bool td_get_domain_chain_id(uint64_t *chain_id);

/**
 * Fill @p addr with the verifyingContract from the domain value tree.
 *
 * @return true if the field was found and copied into @p addr, false otherwise.
 */
bool td_get_domain_contract_addr(uint8_t addr[ADDRESS_LENGTH]);

// Visitor callback for value tree traversal. Return false to abort traversal.
typedef bool (*td_f_value_visitor)(const s_struct_712_value *node, void *context);

/**
 * Traverse domain value tree with visitor callback.
 *
 * @param[in] visitor callback invoked for each node; return false to abort
 * @param[in] context opaque context forwarded to visitor
 * @return whether traversal completed (false if visitor aborted or visitor is NULL)
 */
bool td_traverse_domain(td_f_value_visitor visitor, void *context);

/**
 * Traverse message value tree with visitor callback.
 *
 * @param[in] visitor callback invoked for each node; return false to abort
 * @param[in] context opaque context forwarded to visitor
 * @return whether traversal completed (false if visitor aborted or visitor is NULL)
 */
bool td_traverse_message(td_f_value_visitor visitor, void *context);

/**
 * Release a leaf's raw value once it is no longer needed, keeping the leaf
 * node itself intact within the tree.
 *
 * A node-scoped lifecycle operation, not a traversal — the caller decides when
 * it is safe to release a leaf's raw value (e.g. once it has been displayed and
 * the message hash has already been computed by td_hash_pass(), so the value
 * will never need to be re-read or re-hashed). No-op for VAL_STRUCT/VAL_ARRAY
 * nodes or if the leaf's value was already released.
 *
 * @param[in] node leaf node whose value should be released
 */
void td_release_leaf_value(const s_struct_712_value *node);

// Visitor callback for the declared-structs list traversal. Return false to abort traversal.
typedef bool (*td_f_struct_visitor)(const s_struct_712 *node, void *context);

/**
 * Traverse all declared structs with a visitor callback.
 *
 * @param[in] visitor callback invoked for each struct; return false to abort
 * @param[in] context opaque context forwarded to visitor
 * @return whether traversal completed (false if visitor aborted)
 */
bool td_visit_structs(td_f_struct_visitor visitor, void *context);

// Visitor callback for a struct's field-list traversal. Return false to abort traversal.
typedef bool (*td_f_struct_field_visitor)(const s_struct_712_field *node, void *context);

/**
 * Traverse a struct's fields with a visitor callback.
 *
 * @param[in] s struct whose fields are traversed
 * @param[in] visitor callback invoked for each field; return false to abort
 * @param[in] context opaque context forwarded to visitor
 * @return whether traversal completed (false if visitor aborted); true if s is NULL
 */
bool td_visit_struct_fields(const s_struct_712 *s,
                            td_f_struct_field_visitor visitor,
                            void *context);

// Predicate callback for td_find_struct_if. Return true to select node.
typedef bool (*td_f_struct_pred)(const s_struct_712 *node, const void *context);

/**
 * Find first declared struct satisfying a predicate
 *
 * @param[in] pred predicate called for each declared struct
 * @param[in] ctx opaque context forwarded to pred
 * @return pointer to the first matching struct, or NULL if none found
 */
const s_struct_712 *td_find_struct_if(td_f_struct_pred pred, const void *ctx);

// Predicate callback for td_find_struct_field_if. Return true to select node.
typedef bool (*td_f_struct_field_pred)(const s_struct_712_field *node, const void *context);

/**
 * Find first field of a given struct satisfying a predicate
 *
 * @param[in] s struct whose fields are searched
 * @param[in] pred predicate called for each field
 * @param[in] ctx opaque context forwarded to pred
 * @return pointer to the first matching field, or NULL if none found or s is NULL
 */
const s_struct_712_field *td_find_struct_field_if(const s_struct_712 *s,
                                                  td_f_struct_field_pred pred,
                                                  const void *ctx);
