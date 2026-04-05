#include "notifier.h"

void notifier_chain_init(notifier_chain_t *chain)
{
    list_init(&chain->head);
}

void notifier_block_init(notifier_block_t *nb, notifier_func_t func, int priority)
{
    nb->func = func;
    nb->priority = priority;
    list_init(&nb->node);
}

lv_error_t notifier_chain_register(notifier_chain_t *chain, notifier_block_t *nb)
{
    if (!chain || !nb) {
        return LV_ERROR_INVALID_ARG;
    }

    notifier_block_t *curr;
    list_for_each_entry(curr, &chain->head, node) {
        if (nb->priority > curr->priority) {
            list_add_tail(&nb->node, &curr->node);
            return LV_OK;
        }
    }

    list_add_tail(&nb->node, &chain->head);
    return LV_OK;
}

lv_error_t notifier_chain_unregister(notifier_chain_t *chain, notifier_block_t *nb)
{
    if (!chain || !nb) {
        return LV_ERROR_INVALID_ARG;
    }

    list_del(&nb->node);
    return LV_OK;
}

notify_result_t notifier_chain_notify(notifier_chain_t *chain, void *data)
{
    if (!chain) {
        return NOTIFY_BAD;
    }

    notify_result_t ret = NOTIFY_OK;
    notifier_block_t *nb;
    list_for_each_entry(nb, &chain->head, node) {
        ret = nb->func(data);
        if (ret == NOTIFY_STOP || ret == NOTIFY_BAD) {
            break;
        }
    }

    return ret;
}
