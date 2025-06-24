#pragma once

#include <stdlib.h>
#include <stdbool.h>

/*
 * Forward list (singly-linked)
 */
typedef struct flist_node {
    struct flist_node *next;
} s_flist_node;

typedef void (*f_list_node_del)(s_flist_node *node);
typedef bool (*f_list_node_cmp)(const s_flist_node *a, const s_flist_node *b);

/**
 * Add a new node at the front of the list
 *
 * @param[in,out] list pointer to the list
 * @param[out] node new node to add
 */
void flist_push_front(s_flist_node **list, s_flist_node *node);

/**
 * Remove the first node from the list
 *
 * @param[in,out] list pointer to the list
 * @param[in] func pointer to the node deletion function
 */
void flist_pop_front(s_flist_node **list, f_list_node_del func);

/**
 * Add a new node at the back of the list
 *
 * @param[in,out] list pointer to the list
 * @param[in,out] node new node to add
 */
void flist_push_back(s_flist_node **list, s_flist_node *node);

/**
 * Remove the last node from the list
 *
 * @param[in,out] list pointer to the list
 * @param[in] func pointer to the node deletion function
 */
void flist_pop_back(s_flist_node **list, f_list_node_del func);

/**
 * Insert a new node after a given list node (reference)
 *
 * @param[] list pointer to the list
 * @param[in,out] ref reference node
 * @param[in,out] node new node to add
 */
void flist_insert_after(s_flist_node **list, s_flist_node *ref, s_flist_node *node);

/**
 * Remove a given node from the list
 *
 * @param[in,out] list pointer to the list
 * @param[out] node node to remove
 * @param[in] func pointer to the node deletion function
 */
void flist_remove(s_flist_node **list, s_flist_node *node, f_list_node_del func);

/**
 * Remove all nodes from the list
 *
 * @param[in,out] list pointer to the list
 * @param[in] func pointer to the node deletion function
 */
void flist_clear(s_flist_node **list, f_list_node_del func);

/**
 * Get the list size
 *
 * @param[in] list pointer to the list
 */
size_t flist_size(s_flist_node *const *list);

/**
 * Sort the list
 *
 * @param[in,out] list pointer to the list
 * @param[in] func pointer to the node comparison function
 */
void flist_sort(s_flist_node **list, f_list_node_cmp func);

/*
 * List (doubly-linked)
 * TODO: add functions
 */
typedef struct list_node {
    s_flist_node _list;
    struct list_node *prev;
} s_list_node;
