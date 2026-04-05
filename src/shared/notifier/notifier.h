#ifndef NOTIFIER_H
#define NOTIFIER_H

#include "../../include/local_video.h"
#include "../list/list.h"

/* Notifier return codes */
typedef enum {
    NOTIFY_OK = 0,
    NOTIFY_STOP = 1,
    NOTIFY_BAD = -1
} notify_result_t;

/* Notifier callback function type */
typedef notify_result_t (*notifier_func_t)(void *data);

/* Notifier entry structure */
typedef struct notifier_block {
    notifier_func_t func;
    int priority;
    list_node_t node;
} notifier_block_t;

/* Notifier chain structure */
struct notifier_chain {
    list_node_t head;
};

typedef struct notifier_chain notifier_chain_t;

/* Initialize a notifier chain */
void notifier_chain_init(notifier_chain_t *chain);

/* Initialize a notifier block */
void notifier_block_init(notifier_block_t *nb, notifier_func_t func, int priority);

/* Register a notifier block to the chain (sorted by priority) */
lv_error_t notifier_chain_register(notifier_chain_t *chain, notifier_block_t *nb);

/* Unregister a notifier block from the chain */
lv_error_t notifier_chain_unregister(notifier_chain_t *chain, notifier_block_t *nb);

/* Notify all registered notifiers with given data */
notify_result_t notifier_chain_notify(notifier_chain_t *chain, void *data);

#endif /* NOTIFIER_H */
