#include "local_video.h"
#include "list.h"
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

/* Test structure with embedded list node */
typedef struct {
    int value;
    list_node_t node;
} test_item_t;

int main(void) {
    printf("=== Phase 1.3: 共享基础设施 - 侵入式链表 单元测试 ===\n\n");

    list_node_t head;
    list_init(&head);

    /* Test 1: list_empty */
    printf("--- 测试 1: 空链表检查 ---\n");
    TEST_ASSERT(list_empty(&head), "AC-001: 新初始化的链表为空");

    /* Test 2: Add one item */
    printf("\n--- 测试 2: 添加单个元素 ---\n");
    test_item_t item1 = { .value = 1 };
    list_init(&item1.node);
    list_add_tail(&item1.node, &head);
    TEST_ASSERT(!list_empty(&head), "AC-002: 添加元素后链表不为空");
    test_item_t *pos1 = container_of(head.next, test_item_t, node);
    TEST_ASSERT(pos1->value == 1, "AC-003: 正确获取嵌入链表的结构体");

    /* Test 3: Add multiple items */
    printf("\n--- 测试 3: 添加多个元素 ---\n");
    test_item_t item2 = { .value = 2 };
    test_item_t item3 = { .value = 3 };
    list_init(&item2.node);
    list_init(&item3.node);
    list_add_tail(&item2.node, &head);
    list_add_tail(&item3.node, &head);

    int expected_values[] = {1, 2, 3};
    int index = 0;
    list_for_each_entry(pos1, &head, node) {
        TEST_ASSERT(pos1->value == expected_values[index], "AC-004: list_for_each_entry 遍历顺序正确");
        index++;
    }
    TEST_ASSERT(index == 3, "AC-005: 遍历所有元素");

    /* Test 4: Delete middle item */
    printf("\n--- 测试 4: 删除中间元素 ---\n");
    list_del(&item2.node);
    int expected_after_del[] = {1, 3};
    index = 0;
    list_for_each_entry(pos1, &head, node) {
        TEST_ASSERT(pos1->value == expected_after_del[index], "AC-006: 删除后剩余元素正确");
        index++;
    }
    TEST_ASSERT(index == 2, "AC-007: 删除后元素数量正确");

    /* Test 5: list_del_init */
    printf("\n--- 测试 5: list_del_init ---\n");
    list_del_init(&item1.node);
    TEST_ASSERT(list_empty(&item1.node), "AC-008: list_del_init 后节点重新初始化");
    index = 0;
    list_for_each_entry(pos1, &head, node) {
        TEST_ASSERT(pos1->value == 3, "AC-009: 删除头节点后正确");
        index++;
    }
    TEST_ASSERT(index == 1, "AC-010: 删除后剩余一个元素");

    /* Test 6: list_add */
    printf("\n--- 测试 6: list_add (添加到头部) ---\n");
    test_item_t item4 = { .value = 4 };
    list_init(&item4.node);
    list_add(&item4.node, &head);
    int expected_after_add[] = {4, 3};
    index = 0;
    list_for_each_entry(pos1, &head, node) {
        TEST_ASSERT(pos1->value == expected_after_add[index], "AC-011: list_add 添加到头部顺序正确");
        index++;
    }

    /* Test 7: list_for_each_entry_safe */
    printf("\n--- 测试 7: list_for_each_entry_safe (安全删除所有元素) ---\n");
    test_item_t *n;
    list_for_each_entry_safe(pos1, n, &head, node) {
        list_del(&pos1->node);
    }
    TEST_ASSERT(list_empty(&head), "AC-012: 安全删除后链表为空");

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
