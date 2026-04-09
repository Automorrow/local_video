#include "local_video.h"
#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include "../../src/include/platform.h"
#else
#include <unistd.h>
#endif

static int test_results = 0;
static int test_count = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        test_count++; \
        if (cond) { \
            printf("  [PASS] %s\n", msg); \
        } else { \
            printf("  [FAIL] %s\n", msg); \
            test_results++; \
        } \
    } while (0)

static int shared_counter = 0;
static lv_mutex_t counter_mutex;
static lv_cond_t counter_cond;
static int cond_signaled = 0;

static void *increment_thread(void *arg)
{
    (void)arg;
    for (int i = 0; i < 1000; i++) {
        lv_mutex_lock(&counter_mutex);
        shared_counter++;
        lv_mutex_unlock(&counter_mutex);
    }
    return NULL;
}

static void *signal_thread(void *arg)
{
    (void)arg;
    sleep(1);
    lv_mutex_lock(&counter_mutex);
    cond_signaled = 1;
    lv_cond_signal(&counter_cond);
    lv_mutex_unlock(&counter_mutex);
    return NULL;
}

int main(void)
{
    printf("=== Phase 1.6: 共享基础设施 - 线程安全工具 单元测试 ===\n\n");

    /* Test 1: Mutex initialization */
    printf("--- 测试 1: 互斥锁初始化 ---\n");
    lv_error_t ret;
    ret = lv_mutex_init(&counter_mutex);
    TEST_ASSERT(ret == LV_OK, "AC-001: 互斥锁初始化成功");

    /* Test 2: Mutex lock/unlock */
    printf("\n--- 测试 2: 互斥锁加锁/解锁 ---\n");
    ret = lv_mutex_lock(&counter_mutex);
    TEST_ASSERT(ret == LV_OK, "AC-002: 互斥锁加锁成功");
    shared_counter = 42;
    ret = lv_mutex_unlock(&counter_mutex);
    TEST_ASSERT(ret == LV_OK, "AC-002: 互斥锁解锁成功");
    TEST_ASSERT(shared_counter == 42, "AC-002: 加锁期间修改共享数据成功");

    /* Test 3: Mutex with threads */
    printf("\n--- 测试 3: 多线程互斥锁保护 ---\n");
    shared_counter = 0;
    pthread_t t1, t2;
    pthread_create(&t1, NULL, increment_thread, NULL);
    pthread_create(&t2, NULL, increment_thread, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    TEST_ASSERT(shared_counter == 2000, "AC-003: 多线程计数器正确");

    /* Test 4: Condition variable init */
    printf("\n--- 测试 4: 条件变量初始化 ---\n");
    ret = lv_cond_init(&counter_cond);
    TEST_ASSERT(ret == LV_OK, "AC-004: 条件变量初始化成功");

    /* Test 5: Condition variable wait/signal */
    printf("\n--- 测试 5: 条件变量等待/信号 ---\n");
    cond_signaled = 0;
    pthread_t t3;
    pthread_create(&t3, NULL, signal_thread, NULL);
    lv_mutex_lock(&counter_mutex);
    while (!cond_signaled) {
        lv_cond_wait(&counter_cond, &counter_mutex);
    }
    lv_mutex_unlock(&counter_mutex);
    pthread_join(t3, NULL);
    TEST_ASSERT(cond_signaled == 1, "AC-005: 条件变量信号成功唤醒");

    /* Test 6: Cleanup */
    printf("\n--- 测试 6: 清理资源 ---\n");
    ret = lv_cond_destroy(&counter_cond);
    TEST_ASSERT(ret == LV_OK, "AC-006: 条件变量销毁成功");
    ret = lv_mutex_destroy(&counter_mutex);
    TEST_ASSERT(ret == LV_OK, "AC-006: 互斥锁销毁成功");

    /* Test 7: Invalid arguments */
    printf("\n--- 测试 7: 无效参数处理 ---\n");
    ret = lv_mutex_init(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-007: NULL 互斥锁返回无效参数");
    ret = lv_mutex_lock(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-007: NULL 加锁返回无效参数");
    ret = lv_mutex_unlock(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-007: NULL 解锁返回无效参数");
    ret = lv_mutex_destroy(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-007: NULL 销毁返回无效参数");
    ret = lv_cond_init(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-007: NULL 条件变量返回无效参数");
    ret = lv_cond_wait(NULL, &counter_mutex);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-007: NULL 条件变量等待返回无效参数");
    ret = lv_cond_wait(&counter_cond, NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-007: NULL 互斥锁等待返回无效参数");
    ret = lv_cond_signal(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-007: NULL 信号返回无效参数");
    ret = lv_cond_broadcast(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-007: NULL 广播返回无效参数");
    ret = lv_cond_destroy(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-007: NULL 条件变量销毁返回无效参数");

    printf("\n=== 测试总结 ===\n");
    printf("总测试数: %d\n", test_count);
    printf("通过: %d\n", test_count - test_results);
    printf("失败: %d\n", test_results);

    if (test_results == 0) {
        printf("\n✅ 所有测试通过！\n");
        return EXIT_SUCCESS;
    } else {
        printf("\n❌ 部分测试失败！\n");
        return EXIT_FAILURE;
    }
}
