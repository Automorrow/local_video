#ifndef LIST_H
#define LIST_H

#include "local_video.h"

/* Intrusive linked list node */
struct list_node {
    struct list_node *next;
    struct list_node *prev;
};

typedef struct list_node list_node_t;

/* Initialize a list node */
static inline void list_init(list_node_t *node)
{
    node->next = node;
    node->prev = node;
}

/* Check if a list is empty */
static inline bool list_empty(const list_node_t *head)
{
    return head->next == head;
}

/* Add a node after the given head */
static inline void list_add(list_node_t *node, list_node_t *head)
{
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}

/* Add a node before the given head (add to tail) */
static inline void list_add_tail(list_node_t *node, list_node_t *head)
{
    node->next = head;
    node->prev = head->prev;
    head->prev->next = node;
    head->prev = node;
}

/* Delete a node from the list */
static inline void list_del(list_node_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = NULL;
    node->prev = NULL;
}

/* Delete a node from the list and reinitialize it */
static inline void list_del_init(list_node_t *node)
{
    list_del(node);
    list_init(node);
}

/* Container of macro - get the structure containing the list node */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* List for each entry macro */
#define list_for_each_entry(pos, head, member) \
    for (pos = container_of((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = container_of(pos->member.next, typeof(*pos), member))

/* List for each entry safe macro (safe against deletion) */
#define list_for_each_entry_safe(pos, n, head, member) \
    for (pos = container_of((head)->next, typeof(*pos), member), \
         n = container_of(pos->member.next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = n, n = container_of(n->member.next, typeof(*n), member))

#endif /* LIST_H */
