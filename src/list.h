/**
 * @file
 */

#pragma once

#include <stdlib.h>
#include <stdbool.h>

/**
 * Forward list (singly-linked)
 */
typedef struct flist_node {
    struct flist_node *next;
} s_flist_node;

/**
 * List (doubly-linked)
 */
typedef struct list_node {
    s_flist_node _list;
    struct list_node *prev;
} s_list_node;

/**
 * Node deletion function
 */
typedef void (*f_list_node_del)(s_flist_node *node);

/**
 * Node comparison function
 */
typedef bool (*f_list_node_cmp)(const s_flist_node *a, const s_flist_node *b);

/**
 * Add a node at the beginning of the list
 *
 * @param[in,out] list pointer to the list
 * @param[out] node new node to add
 */
void flist_push_front(s_flist_node **list, s_flist_node *node);

/**
 * Remove the first node from the list
 *
 * @param[in,out] list pointer to the list
 * @param[in] del_func node deletion function
 */
void flist_pop_front(s_flist_node **list, f_list_node_del del_func);

/**
 * Add a node at the end of the list
 *
 * @param[in,out] list pointer to the list
 * @param[in,out] node new node to add
 */
void flist_push_back(s_flist_node **list, s_flist_node *node);

/**
 * Remove the last node from the list
 *
 * @param[in,out] list pointer to the list
 * @param[in] del_func node deletion function
 */
void flist_pop_back(s_flist_node **list, f_list_node_del del_func);

/**
 * Insert a new node after a given list node (reference)
 *
 * @param list pointer to the list
 * @param[in,out] ref reference node
 * @param[in,out] node new node to add
 */
void flist_insert_after(s_flist_node **list, s_flist_node *ref, s_flist_node *node);

/**
 * Remove a given node from the list
 *
 * @param[in,out] list pointer to the list
 * @param[out] node node to remove
 * @param[in] del_func node deletion function
 */
void flist_remove(s_flist_node **list, s_flist_node *node, f_list_node_del del_func);

/**
 * Remove all nodes from the list
 *
 * @param[in,out] list pointer to the list
 * @param[in] del_func node deletion function
 */
void flist_clear(s_flist_node **list, f_list_node_del del_func);

/**
 * Get the list size
 *
 * @param[in] list pointer to the list
 * @return number of nodes in the list
 */
size_t flist_size(s_flist_node *const *list);

/**
 * Sort the list
 *
 * @param[in,out] list pointer to the list
 * @param[in] cmp_func pointer to the node comparison function
 */
void flist_sort(s_flist_node **list, f_list_node_cmp cmp_func);

/// @copydoc flist_push_front(s_flist_node **, s_flist_node *)
void list_push_front(s_list_node **list, s_list_node *node);

/// @copydoc flist_pop_front(s_flist_node **, f_list_node_del)
void list_pop_front(s_list_node **list, f_list_node_del del_func);

/// @copydoc flist_push_back(s_flist_node **, s_flist_node *)
void list_push_back(s_list_node **list, s_list_node *node);

/// @copydoc flist_pop_back(s_flist_node **, f_list_node_del)
void list_pop_back(s_list_node **list, f_list_node_del del_func);

/**
 * Insert a new node before a given list node (reference)
 *
 * @param[in,out] list pointer to the list
 * @param[in,out] ref reference node
 * @param[in,out] node new node to add
 */
void list_insert_before(s_list_node **list, s_list_node *ref, s_list_node *node);

/// @copydoc flist_insert_after(s_flist_node **, s_flist_node *, s_flist_node *)
void list_insert_after(s_list_node **list, s_list_node *ref, s_list_node *node);

/// @copydoc flist_remove(s_flist_node **, s_flist_node *, f_list_node_del)
void list_remove(s_list_node **list, s_list_node *node, f_list_node_del del_func);

/// @copydoc flist_clear(s_flist_node **, f_list_node_del)
void list_clear(s_list_node **list, f_list_node_del del_func);

/// @copydoc flist_size(s_flist_node *const *)
size_t list_size(s_list_node *const *list);

/// @copydoc flist_sort(s_flist_node **, f_list_node_cmp)
void list_sort(s_list_node **list, f_list_node_cmp cmp_func);
