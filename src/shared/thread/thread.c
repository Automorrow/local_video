#include "thread.h"

lv_error_t lv_mutex_init(lv_mutex_t *mutex)
{
    if (!mutex) {
        return LV_ERROR_INVALID_ARG;
    }

    int ret = pthread_mutex_init(&mutex->mutex, NULL);
    if (ret != 0) {
        return LV_ERROR_UNKNOWN;
    }

    return LV_OK;
}

lv_error_t lv_mutex_lock(lv_mutex_t *mutex)
{
    if (!mutex) {
        return LV_ERROR_INVALID_ARG;
    }

    int ret = pthread_mutex_lock(&mutex->mutex);
    if (ret != 0) {
        return LV_ERROR_UNKNOWN;
    }

    return LV_OK;
}

lv_error_t lv_mutex_unlock(lv_mutex_t *mutex)
{
    if (!mutex) {
        return LV_ERROR_INVALID_ARG;
    }

    int ret = pthread_mutex_unlock(&mutex->mutex);
    if (ret != 0) {
        return LV_ERROR_UNKNOWN;
    }

    return LV_OK;
}

lv_error_t lv_mutex_destroy(lv_mutex_t *mutex)
{
    if (!mutex) {
        return LV_ERROR_INVALID_ARG;
    }

    int ret = pthread_mutex_destroy(&mutex->mutex);
    if (ret != 0) {
        return LV_ERROR_UNKNOWN;
    }

    return LV_OK;
}

lv_error_t lv_cond_init(lv_cond_t *cond)
{
    if (!cond) {
        return LV_ERROR_INVALID_ARG;
    }

    int ret = pthread_cond_init(&cond->cond, NULL);
    if (ret != 0) {
        return LV_ERROR_UNKNOWN;
    }

    return LV_OK;
}

lv_error_t lv_cond_wait(lv_cond_t *cond, lv_mutex_t *mutex)
{
    if (!cond || !mutex) {
        return LV_ERROR_INVALID_ARG;
    }

    int ret = pthread_cond_wait(&cond->cond, &mutex->mutex);
    if (ret != 0) {
        return LV_ERROR_UNKNOWN;
    }

    return LV_OK;
}

lv_error_t lv_cond_signal(lv_cond_t *cond)
{
    if (!cond) {
        return LV_ERROR_INVALID_ARG;
    }

    int ret = pthread_cond_signal(&cond->cond);
    if (ret != 0) {
        return LV_ERROR_UNKNOWN;
    }

    return LV_OK;
}

lv_error_t lv_cond_broadcast(lv_cond_t *cond)
{
    if (!cond) {
        return LV_ERROR_INVALID_ARG;
    }

    int ret = pthread_cond_broadcast(&cond->cond);
    if (ret != 0) {
        return LV_ERROR_UNKNOWN;
    }

    return LV_OK;
}

lv_error_t lv_cond_destroy(lv_cond_t *cond)
{
    if (!cond) {
        return LV_ERROR_INVALID_ARG;
    }

    int ret = pthread_cond_destroy(&cond->cond);
    if (ret != 0) {
        return LV_ERROR_UNKNOWN;
    }

    return LV_OK;
}
