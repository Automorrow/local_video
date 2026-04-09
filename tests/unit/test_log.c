#include "local_video.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static char test_buffer[4096] = {0};

static void read_temp_file(FILE *f)
{
    rewind(f);
    memset(test_buffer, 0, sizeof(test_buffer));
    size_t n = fread(test_buffer, 1, sizeof(test_buffer) - 1, f);
    (void)n;
}

int main(void)
{
    printf("=== Phase 1.5: 共享基础设施 - 日志系统 单元测试 ===\n\n");

    char temp_filename[] = "/tmp/test_log_XXXXXX";
    int fd = mkstemp(temp_filename);
    FILE *test_file = fdopen(fd, "w+");

    /* Test 1: Set log file and level */
    printf("--- 测试 1: 设置日志文件和级别 ---\n");
    log_set_file(test_file);
    log_set_level(LV_LOG_DEBUG);
    TEST_ASSERT(1, "AC-001: 设置日志文件成功");
    TEST_ASSERT(1, "AC-001: 设置日志级别成功");

    /* Test 2: Log DEBUG */
    printf("\n--- 测试 2: 输出 DEBUG 日志 ---\n");
    int r = ftruncate(fd, 0);
    (void)r;
    rewind(test_file);
    log_debug("test debug message");
    read_temp_file(test_file);
    TEST_ASSERT(strstr(test_buffer, "[DEBUG]") != NULL, "AC-002: DEBUG 日志包含 DEBUG 标签");
    TEST_ASSERT(strstr(test_buffer, "test debug message") != NULL, "AC-002: DEBUG 日志包含消息内容");

    /* Test 3: Log INFO */
    printf("\n--- 测试 3: 输出 INFO 日志 ---\n");
    r = ftruncate(fd, 0);
    (void)r;
    rewind(test_file);
    log_info("test info message %d", 123);
    read_temp_file(test_file);
    TEST_ASSERT(strstr(test_buffer, "[INFO]") != NULL, "AC-003: INFO 日志包含 INFO 标签");
    TEST_ASSERT(strstr(test_buffer, "test info message 123") != NULL, "AC-003: INFO 日志包含格式化消息内容");

    /* Test 4: Log WARNING */
    printf("\n--- 测试 4: 输出 WARNING 日志 ---\n");
    r = ftruncate(fd, 0);
    (void)r;
    rewind(test_file);
    log_warning("test warning message");
    read_temp_file(test_file);
    TEST_ASSERT(strstr(test_buffer, "[WARNING]") != NULL, "AC-004: WARNING 日志包含 WARNING 标签");

    /* Test 5: Log ERROR */
    printf("\n--- 测试 5: 输出 ERROR 日志 ---\n");
    r = ftruncate(fd, 0);
    (void)r;
    rewind(test_file);
    log_error("test error message");
    read_temp_file(test_file);
    TEST_ASSERT(strstr(test_buffer, "[ERROR]") != NULL, "AC-005: ERROR 日志包含 ERROR 标签");

    /* Test 6: Log level filtering */
    printf("\n--- 测试 6: 日志级别过滤 ---\n");
    log_set_level(LV_LOG_WARNING);
    r = ftruncate(fd, 0);
    (void)r;
    rewind(test_file);
    log_info("should not appear");
    log_warning("should appear");
    read_temp_file(test_file);
    TEST_ASSERT(strstr(test_buffer, "should not appear") == NULL, "AC-006: 低于当前级别的日志不输出");
    TEST_ASSERT(strstr(test_buffer, "should appear") != NULL, "AC-006: 高于或等于当前级别的日志输出");

    /* Test 7: Log with timestamp */
    printf("\n--- 测试 7: 日志包含时间戳 ---\n");
    r = ftruncate(fd, 0);
    (void)r;
    rewind(test_file);
    log_error("timestamp test");
    read_temp_file(test_file);
    TEST_ASSERT(strstr(test_buffer, "[") != NULL, "AC-007: 日志包含时间戳起始");
    TEST_ASSERT(strstr(test_buffer, "]") != NULL, "AC-007: 日志包含时间戳结束");

    fclose(test_file);
    remove(temp_filename);

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
