#include "list.h"

static void push_front_internal(s_flist_node **list, s_flist_node *node, bool doubly_linked) {
    if ((list != NULL) && (node != NULL)) {
        node->next = *list;
        *list = node;
        if (doubly_linked) {
            if (node->next != NULL) {
                ((s_list_node *) node->next)->prev = (s_list_node *) node;
            }
            ((s_list_node *) node)->prev = NULL;
        }
    }
}

void flist_push_front(s_flist_node **list, s_flist_node *node) {
    push_front_internal(list, node, false);
}

static void pop_front_internal(s_flist_node **list, f_list_node_del del_func, bool doubly_linked) {
    s_flist_node *tmp;

    if (list != NULL) {
        tmp = *list;
        if (tmp != NULL) {
            *list = tmp->next;
            if (del_func != NULL) {
                del_func(tmp);
            }
            if (doubly_linked) {
                if (*list != NULL) {
                    (*(s_list_node **) list)->prev = NULL;
                }
            }
        }
    }
}

void flist_pop_front(s_flist_node **list, f_list_node_del del_func) {
    pop_front_internal(list, del_func, false);
}

static void push_back_internal(s_flist_node **list, s_flist_node *node, bool doubly_linked) {
    s_flist_node *tmp;

    if ((list != NULL) && (node != NULL)) {
        node->next = NULL;
        if (*list == NULL) {
            if (doubly_linked) {
                ((s_list_node *) node)->prev = NULL;
            }
            *list = node;
        } else {
            for (tmp = *list; tmp->next != NULL; tmp = tmp->next) {
            }
            if (doubly_linked) {
                ((s_list_node *) node)->prev = (s_list_node *) tmp;
            }
            tmp->next = node;
        }
    }
}

void flist_push_back(s_flist_node **list, s_flist_node *node) {
    push_back_internal(list, node, false);
}

void flist_pop_back(s_flist_node **list, f_list_node_del del_func) {
    s_flist_node *tmp;

    if (list != NULL) {
        tmp = *list;
        if (tmp != NULL) {
            // only one element
            if (tmp->next == NULL) {
                flist_pop_front(list, del_func);
            } else {
                while (tmp->next->next != NULL) {
                    tmp = tmp->next;
                }
                if (del_func != NULL) {
                    del_func(tmp->next);
                }
                tmp->next = NULL;
            }
        }
    }
}

void insert_after_internal(s_flist_node *ref, s_flist_node *node, bool doubly_linked) {
    if ((ref != NULL) && (node != NULL)) {
        if (doubly_linked) {
            if (ref->next != NULL) {
                ((s_list_node *) (ref->next))->prev = (s_list_node *) node;
            }
            ((s_list_node *) node)->prev = (s_list_node *) ref;
        }
        node->next = ref->next;
        ref->next = node;
    }
}

void flist_insert_after(s_flist_node **list, s_flist_node *ref, s_flist_node *node) {
    (void) list;
    insert_after_internal(ref, node, false);
}

static void remove_internal(s_flist_node **list,
                            s_flist_node *node,
                            f_list_node_del del_func,
                            bool doubly_linked) {
    s_flist_node *it;
    s_flist_node *tmp;

    if ((list != NULL) && (node != NULL)) {
        if (node == *list) {
            // first element
            pop_front_internal(list, del_func, doubly_linked);
        } else {
            it = *list;
            if (it != NULL) {
                while ((it->next != node) && (it->next != NULL)) {
                    it = it->next;
                }
                if (it->next == NULL) {
                    // node not found
                    return;
                }
                tmp = it->next->next;
                if (doubly_linked) {
                    if (tmp != NULL) {
                        ((s_list_node *) tmp)->prev = ((s_list_node *) node)->prev;
                    }
                }
                if (del_func != NULL) {
                    del_func(it->next);
                }
                it->next = tmp;
            }
        }
    }
}

void flist_remove(s_flist_node **list, s_flist_node *node, f_list_node_del del_func) {
    remove_internal(list, node, del_func, false);
}

static size_t remove_if_internal(s_flist_node **list,
                                 f_list_node_pred pred_func,
                                 f_list_node_del del_func,
                                 bool doubly_linked) {
    s_flist_node *node;
    s_flist_node *tmp;
    size_t count = 0;

    if ((list != NULL) && (pred_func != NULL)) {
        node = *list;
        while (node != NULL) {
            tmp = node->next;
            if (pred_func(node)) {
                remove_internal(list, node, del_func, doubly_linked);
            }
            node = tmp;
        }
    }
    return count;
}

size_t flist_remove_if(s_flist_node **list, f_list_node_pred pred_func, f_list_node_del del_func) {
    return remove_if_internal(list, pred_func, del_func, false);
}

void flist_clear(s_flist_node **list, f_list_node_del del_func) {
    s_flist_node *tmp;
    s_flist_node *next;

    if (list != NULL) {
        tmp = *list;
        while (tmp != NULL) {
            next = tmp->next;
            if (del_func != NULL) {
                del_func(tmp);
            }
            tmp = next;
        }
        *list = NULL;
    }
}

size_t flist_size(s_flist_node *const *list) {
    size_t size = 0;

    if (list != NULL) {
        for (s_flist_node *tmp = *list; tmp != NULL; tmp = tmp->next) {
            size += 1;
        }
    }
    return size;
}

bool flist_empty(s_flist_node *const *list) {
    return flist_size(list) == 0;
}

static void sort_internal(s_flist_node **list, f_list_node_cmp cmp_func, bool doubly_linked) {
    s_flist_node **tmp;
    s_flist_node *a, *b;
    bool sorted;

    if ((list != NULL) && (cmp_func != NULL)) {
        do {
            sorted = true;
            for (tmp = list; (*tmp != NULL) && ((*tmp)->next != NULL); tmp = &(*tmp)->next) {
                a = *tmp;
                b = a->next;
                if (cmp_func(a, b) == false) {
                    *tmp = b;
                    a->next = b->next;
                    b->next = a;
                    if (doubly_linked) {
                        ((s_list_node *) b)->prev = ((s_list_node *) a)->prev;
                        ((s_list_node *) a)->prev = (s_list_node *) b;
                        if (a->next != NULL) {
                            ((s_list_node *) a->next)->prev = (s_list_node *) a;
                        }
                    }
                    sorted = false;
                }
            }
        } while (!sorted);
    }
}

void flist_sort(s_flist_node **list, f_list_node_cmp cmp_func) {
    sort_internal(list, cmp_func, false);
}

static size_t unique_internal(s_flist_node **list,
                              f_list_node_bin_pred pred_func,
                              f_list_node_del del_func,
                              bool doubly_linked) {
    size_t count = 0;

    if ((list != NULL) && (pred_func != NULL)) {
        for (s_flist_node *ref = *list; ref != NULL; ref = ref->next) {
            s_flist_node *node = ref->next;
            while ((node != NULL) && (pred_func(ref, node))) {
                s_flist_node *tmp = node->next;
                remove_internal(list, node, del_func, doubly_linked);
                count += 1;
                node = tmp;
            }
        }
    }
    return count;
}

size_t flist_unique(s_flist_node **list, f_list_node_bin_pred pred_func, f_list_node_del del_func) {
    return unique_internal(list, pred_func, del_func, false);
}

void list_push_front(s_list_node **list, s_list_node *node) {
    push_front_internal((s_flist_node **) list, (s_flist_node *) node, true);
}

void list_pop_front(s_list_node **list, f_list_node_del del_func) {
    pop_front_internal((s_flist_node **) list, del_func, true);
}

void list_push_back(s_list_node **list, s_list_node *node) {
    push_back_internal((s_flist_node **) list, (s_flist_node *) node, true);
}

void list_pop_back(s_list_node **list, f_list_node_del del_func) {
    flist_pop_back((s_flist_node **) list, del_func);
}

void list_insert_before(s_list_node **list, s_list_node *ref, s_list_node *node) {
    if ((ref != NULL) && (node != NULL)) {
        if (ref->prev == NULL) {
            if ((list != NULL) && (*list == ref)) {
                list_push_front(list, node);
            }
        } else {
            list_insert_after(list, ref->prev, node);
        }
    }
}

void list_insert_after(s_list_node **list, s_list_node *ref, s_list_node *node) {
    (void) list;
    insert_after_internal((s_flist_node *) ref, (s_flist_node *) node, true);
}

void list_remove(s_list_node **list, s_list_node *node, f_list_node_del del_func) {
    remove_internal((s_flist_node **) list, (s_flist_node *) node, del_func, true);
}

size_t list_remove_if(s_list_node **list, f_list_node_pred pred_func, f_list_node_del del_func) {
    return remove_if_internal((s_flist_node **) list, pred_func, del_func, true);
}

void list_clear(s_list_node **list, f_list_node_del del_func) {
    flist_clear((s_flist_node **) list, del_func);
}

size_t list_size(s_list_node *const *list) {
    return flist_size((s_flist_node **) list);
}

bool list_empty(s_list_node *const *list) {
    return list_size(list) == 0;
}

void list_sort(s_list_node **list, f_list_node_cmp del_func) {
    sort_internal((s_flist_node **) list, del_func, true);
}

size_t list_unique(s_list_node **list, f_list_node_bin_pred pred_func, f_list_node_del del_func) {
    return unique_internal((s_flist_node **) list, pred_func, del_func, true);
}
