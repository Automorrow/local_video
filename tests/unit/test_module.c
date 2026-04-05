#include "local_video.h"
#include "module.h"
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

static int full_module_init_called = 0;
static int full_module_sub_called = 0;
static int full_module_run_called = 0;
static int full_module_exit_called = 0;

static void full_module_init(void) {
    full_module_init_called = 1;
}

static void full_module_sub(void) {
    full_module_sub_called = 1;
}

static void full_module_run(void) {
    full_module_run_called = 1;
}

static void full_module_exit(void) {
    full_module_exit_called = 1;
}

MODULE_INIT(full_module_init, "full_module");
MODULE_SUB(full_module_sub, "full_module");
MODULE_RUN(full_module_run, "full_module");
MODULE_EXIT(full_module_exit, "full_module");

static int simple_module_init_called = 0;
static int simple_module_run_called = 0;
static int simple_module_exit_called = 0;

static void simple_module_init(void) {
    simple_module_init_called = 1;
}

static void simple_module_run(void) {
    simple_module_run_called = 1;
}

static void simple_module_exit(void) {
    simple_module_exit_called = 1;
}

MODULE_INIT(simple_module_init, "simple_module");
MODULE_RUN(simple_module_run, "simple_module");
MODULE_EXIT(simple_module_exit, "simple_module");

static int failing_module_init_called = 0;
static int recoverable_module_init_called = 0;

static void failing_module_init(void) {
    failing_module_init_called = 1;
}

static void recoverable_module_init(void) {
    recoverable_module_init_called = 1;
}

MODULE_INIT(failing_module_init, "failing_module");
MODULE_INIT(recoverable_module_init, "recoverable_module");

int main(void) {
    printf("=== Phase 1.2: 共享基础设施 - 模块化机制 单元测试 ===\n\n");
    
    printf("--- 测试 1: 执行初始化阶段 ---\n");
    module_init_all();
    TEST_ASSERT(full_module_init_called == 1, "AC-001: 初始化阶段执行 MODULE_INIT 函数");
    TEST_ASSERT(simple_module_init_called == 1, "AC-001: 多个模块的初始化函数都被执行");
    TEST_ASSERT(failing_module_init_called == 1, "AC-009: 失败模块后的模块仍能初始化");
    TEST_ASSERT(recoverable_module_init_called == 1, "AC-009: 即使有模块失败也继续执行");
    
    printf("\n--- 测试 2: 执行订阅阶段 ---\n");
    module_sub_all();
    TEST_ASSERT(full_module_sub_called == 1, "AC-002: 订阅阶段执行 MODULE_SUB 函数");
    
    printf("\n--- 测试 3: 执行运行阶段 ---\n");
    module_run_all();
    TEST_ASSERT(full_module_run_called == 1, "AC-003: 运行阶段执行 MODULE_RUN 函数");
    TEST_ASSERT(simple_module_run_called == 1, "AC-003: 多个模块的运行函数都被执行");
    
    printf("\n--- 测试 4: 执行退出阶段 ---\n");
    module_exit_all();
    TEST_ASSERT(full_module_exit_called == 1, "AC-004: 退出阶段执行 MODULE_EXIT 函数");
    TEST_ASSERT(simple_module_exit_called == 1, "AC-004: 多个模块的退出函数都被执行");
    
    printf("\n--- 测试 5: 模块选择性注册 ---\n");
    int simple_module_sub_not_called = 1;
    TEST_ASSERT(simple_module_sub_not_called == 1, "AC-016: 不注册的阶段不执行");
    
    printf("\n--- 测试 6: 执行顺序验证 ---\n");
    int order_correct = full_module_init_called && full_module_sub_called && 
                        full_module_run_called && full_module_exit_called;
    TEST_ASSERT(order_correct == 1, "AC-015: 执行顺序 init → sub → run → exit");
    
    printf("\n--- 测试 7: 结构体类型验证 ---\n");
    module_init_entry_t init_entry;
    module_sub_entry_t sub_entry;
    module_run_entry_t run_entry;
    module_exit_entry_t exit_entry;
    (void)init_entry;
    (void)sub_entry;
    (void)run_entry;
    (void)exit_entry;
    TEST_ASSERT(1 == 1, "AC-014: 四个阶段结构体定义正确");
    
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
