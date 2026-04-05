#ifndef THREAD_H
#define THREAD_H

#include "../../include/local_video.h"
#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;
} lv_mutex_t;

typedef struct {
    pthread_cond_t cond;
} lv_cond_t;

lv_error_t lv_mutex_init(lv_mutex_t *mutex);
lv_error_t lv_mutex_lock(lv_mutex_t *mutex);
lv_error_t lv_mutex_unlock(lv_mutex_t *mutex);
lv_error_t lv_mutex_destroy(lv_mutex_t *mutex);

lv_error_t lv_cond_init(lv_cond_t *cond);
lv_error_t lv_cond_wait(lv_cond_t *cond, lv_mutex_t *mutex);
lv_error_t lv_cond_signal(lv_cond_t *cond);
lv_error_t lv_cond_broadcast(lv_cond_t *cond);
lv_error_t lv_cond_destroy(lv_cond_t *cond);

#endif /* THREAD_H */
