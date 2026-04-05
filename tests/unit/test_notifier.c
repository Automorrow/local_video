#include "local_video.h"
#include "notifier.h"
#include <stdio.h>
#include <stdlib.h>

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

static int test1_called = 0;
static int test2_called = 0;
static int test3_called = 0;
static int call_order[3] = {0};
static int call_index = 0;

static notify_result_t test_notifier1(void *data)
{
    test1_called = 1;
    call_order[call_index++] = 1;
    (void)data;
    return NOTIFY_OK;
}

static notify_result_t test_notifier2(void *data)
{
    test2_called = 1;
    call_order[call_index++] = 2;
    (void)data;
    return NOTIFY_OK;
}

static notify_result_t test_notifier3(void *data)
{
    test3_called = 1;
    call_order[call_index++] = 3;
    (void)data;
    return NOTIFY_OK;
}

static notify_result_t stop_notifier(void *data)
{
    (void)data;
    return NOTIFY_STOP;
}

static notify_result_t bad_notifier(void *data)
{
    (void)data;
    return NOTIFY_BAD;
}

static int data_test = 0;

static notify_result_t data_notifier(void *data)
{
    if (data) {
        data_test = *(int *)data;
    }
    return NOTIFY_OK;
}

int main(void)
{
    printf("=== Phase 1.4: 共享基础设施 - 通知链 单元测试 ===\n\n");

    /* Test 1: Initialize chain and register notifiers */
    printf("--- 测试 1: 初始化通知链并注册通知 ---\n");
    notifier_chain_t chain;
    notifier_chain_init(&chain);
    TEST_ASSERT(list_empty(&chain.head), "AC-001: 新初始化的通知链为空");

    notifier_block_t nb1, nb2, nb3;
    notifier_block_init(&nb1, test_notifier1, 10);
    notifier_block_init(&nb2, test_notifier2, 30);
    notifier_block_init(&nb3, test_notifier3, 20);

    lv_error_t ret;
    ret = notifier_chain_register(&chain, &nb1);
    TEST_ASSERT(ret == LV_OK, "AC-002: 注册通知成功");
    ret = notifier_chain_register(&chain, &nb2);
    TEST_ASSERT(ret == LV_OK, "AC-002: 注册多个通知成功");
    ret = notifier_chain_register(&chain, &nb3);
    TEST_ASSERT(ret == LV_OK, "AC-002: 注册多个通知成功");

    /* Test 2: Notify and check priority order */
    printf("\n--- 测试 2: 通知并检查优先级顺序 ---\n");
    test1_called = 0; test2_called = 0; test3_called = 0;
    call_index = 0;
    notify_result_t nret = notifier_chain_notify(&chain, NULL);
    TEST_ASSERT(nret == NOTIFY_OK, "AC-003: 通知返回 OK");
    TEST_ASSERT(test1_called == 1, "AC-004: 优先级10的通知被调用");
    TEST_ASSERT(test2_called == 1, "AC-004: 优先级30的通知被调用");
    TEST_ASSERT(test3_called == 1, "AC-004: 优先级20的通知被调用");
    TEST_ASSERT(call_order[0] == 2, "AC-005: 优先级最高的先调用");
    TEST_ASSERT(call_order[1] == 3, "AC-005: 按优先级降序调用");
    TEST_ASSERT(call_order[2] == 1, "AC-005: 优先级最低的最后调用");

    /* Test 3: Unregister and notify */
    printf("\n--- 测试 3: 注销通知并重新通知 ---\n");
    ret = notifier_chain_unregister(&chain, &nb2);
    TEST_ASSERT(ret == LV_OK, "AC-006: 注销通知成功");
    test1_called = 0; test2_called = 0; test3_called = 0;
    call_index = 0;
    nret = notifier_chain_notify(&chain, NULL);
    TEST_ASSERT(nret == NOTIFY_OK, "AC-007: 注销后通知仍返回 OK");
    TEST_ASSERT(test1_called == 1, "AC-008: 未注销的通知被调用");
    TEST_ASSERT(test2_called == 0, "AC-008: 已注销的通知不被调用");
    TEST_ASSERT(test3_called == 1, "AC-008: 未注销的通知被调用");
    TEST_ASSERT(call_order[0] == 3, "AC-009: 剩余通知按优先级顺序调用");
    TEST_ASSERT(call_order[1] == 1, "AC-009: 剩余通知按优先级顺序调用");

    /* Test 4: NOTIFY_STOP */
    printf("\n--- 测试 4: NOTIFY_STOP 停止后续通知 ---\n");
    notifier_block_t nb_stop;
    notifier_block_init(&nb_stop, stop_notifier, 25);
    ret = notifier_chain_register(&chain, &nb_stop);
    TEST_ASSERT(ret == LV_OK, "AC-010: 注册 STOP 通知成功");
    test1_called = 0; test3_called = 0;
    nret = notifier_chain_notify(&chain, NULL);
    TEST_ASSERT(nret == NOTIFY_STOP, "AC-011: 通知返回 STOP");
    TEST_ASSERT(test3_called == 0, "AC-012: STOP 后的通知不被调用");
    TEST_ASSERT(test1_called == 0, "AC-012: STOP 后的通知不被调用");

    /* Test 5: NOTIFY_BAD */
    printf("\n--- 测试 5: NOTIFY_BAD 停止后续通知 ---\n");
    notifier_chain_unregister(&chain, &nb_stop);
    notifier_block_t nb_bad;
    notifier_block_init(&nb_bad, bad_notifier, 25);
    ret = notifier_chain_register(&chain, &nb_bad);
    TEST_ASSERT(ret == LV_OK, "AC-013: 注册 BAD 通知成功");
    test1_called = 0; test3_called = 0;
    nret = notifier_chain_notify(&chain, NULL);
    TEST_ASSERT(nret == NOTIFY_BAD, "AC-014: 通知返回 BAD");
    TEST_ASSERT(test3_called == 0, "AC-015: BAD 后的通知不被调用");
    TEST_ASSERT(test1_called == 0, "AC-015: BAD 后的通知不被调用");

    /* Test 6: Data passing */
    printf("\n--- 测试 6: 数据传递 ---\n");
    notifier_chain_unregister(&chain, &nb1);
    notifier_chain_unregister(&chain, &nb3);
    notifier_chain_unregister(&chain, &nb_bad);
    notifier_block_t nb_data;
    notifier_block_init(&nb_data, data_notifier, 10);
    ret = notifier_chain_register(&chain, &nb_data);
    TEST_ASSERT(ret == LV_OK, "AC-016: 注册数据通知成功");
    int test_value = 42;
    data_test = 0;
    nret = notifier_chain_notify(&chain, &test_value);
    TEST_ASSERT(nret == NOTIFY_OK, "AC-017: 数据通知返回 OK");
    TEST_ASSERT(data_test == 42, "AC-018: 数据正确传递");

    /* Test 7: Invalid arguments */
    printf("\n--- 测试 7: 无效参数处理 ---\n");
    ret = notifier_chain_register(NULL, &nb1);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-019: NULL 链返回无效参数");
    ret = notifier_chain_register(&chain, NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-019: NULL 块返回无效参数");
    ret = notifier_chain_unregister(NULL, &nb1);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-019: NULL 链注销返回无效参数");
    ret = notifier_chain_unregister(&chain, NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-019: NULL 块注销返回无效参数");
    nret = notifier_chain_notify(NULL, NULL);
    TEST_ASSERT(nret == NOTIFY_BAD, "AC-019: NULL 链通知返回 BAD");

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
