#include <os/list.h>

list_node_t *list_init(list_node_t *l) {
    l->prev = l->next = l;
    return l;
}

list_node_t *list_insert(list_node_t *l, list_node_t *n) {
    n->next = l->next;
    n->prev = l;
    l->next->prev = n;
    l->next = n;
    return l;
}

list_node_t *list_delete(list_node_t *l) {
    l->next->prev = l->prev;
    l->prev->next = l->next;
    list_node_t *n = l->next;
    l->prev = l->next = l;
    return n;
}

int list_is_empty(list_node_t *l) {
    return l->next == l;
}
