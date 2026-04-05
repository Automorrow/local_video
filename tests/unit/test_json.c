#include "local_video.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    printf("=== Phase 1.7: 共享基础设施 - JSON 库 单元测试 ===\n\n");

    char temp_filename[] = "/tmp/test_json_XXXXXX";
    int fd = mkstemp(temp_filename);
    FILE *test_file = fdopen(fd, "w+");

    /* Test 1: JSON writer init */
    printf("--- 测试 1: JSON 写入器初始化 ---\n");
    json_writer_t writer;
    lv_error_t ret = json_writer_init(&writer, test_file);
    TEST_ASSERT(ret == LV_OK, "AC-001: JSON 写入器初始化成功");

    /* Test 2: Simple object */
    printf("\n--- 测试 2: 简单对象 ---\n");
    int r = ftruncate(fd, 0);
    (void)r;
    rewind(test_file);
    json_writer_init(&writer, test_file);
    json_start_object(&writer);
    json_add_key(&writer, "name");
    json_add_string(&writer, "test video");
    json_add_key(&writer, "id");
    json_add_int(&writer, 123);
    json_add_key(&writer, "available");
    json_add_bool(&writer, true);
    json_add_key(&writer, "metadata");
    json_add_null(&writer);
    json_end_object(&writer);
    read_temp_file(test_file);
    TEST_ASSERT(strstr(test_buffer, "{\"name\":\"test video\"") != NULL, "AC-002: 对象包含字符串字段");
    TEST_ASSERT(strstr(test_buffer, "\"id\":123") != NULL, "AC-003: 对象包含整数字段");
    TEST_ASSERT(strstr(test_buffer, "\"available\":true") != NULL, "AC-004: 对象包含布尔字段");
    TEST_ASSERT(strstr(test_buffer, "\"metadata\":null") != NULL, "AC-005: 对象包含 null 字段");

    /* Test 3: Array */
    printf("\n--- 测试 3: 数组 ---\n");
    r = ftruncate(fd, 0);
    (void)r;
    rewind(test_file);
    json_writer_init(&writer, test_file);
    json_start_array(&writer);
    json_add_int(&writer, 1);
    json_add_int(&writer, 2);
    json_add_int(&writer, 3);
    json_end_array(&writer);
    read_temp_file(test_file);
    TEST_ASSERT(strcmp(test_buffer, "[1,2,3]") == 0, "AC-006: 数组格式正确");

    /* Test 4: Nested objects/arrays */
    printf("\n--- 测试 4: 嵌套对象/数组 ---\n");
    r = ftruncate(fd, 0);
    (void)r;
    rewind(test_file);
    json_writer_init(&writer, test_file);
    json_start_object(&writer);
    json_add_key(&writer, "videos");
    json_start_array(&writer);
    json_start_object(&writer);
    json_add_key(&writer, "title");
    json_add_string(&writer, "video 1");
    json_end_object(&writer);
    json_start_object(&writer);
    json_add_key(&writer, "title");
    json_add_string(&writer, "video 2");
    json_end_object(&writer);
    json_end_array(&writer);
    json_end_object(&writer);
    read_temp_file(test_file);
    TEST_ASSERT(strstr(test_buffer, "\"videos\":[{\"title\":\"video 1\"},{\"title\":\"video 2\"}]") != NULL, "AC-007: 嵌套结构正确");

    /* Test 5: String escaping */
    printf("\n--- 测试 5: 字符串转义 ---\n");
    r = ftruncate(fd, 0);
    (void)r;
    rewind(test_file);
    json_writer_init(&writer, test_file);
    json_start_object(&writer);
    json_add_key(&writer, "text");
    json_add_string(&writer, "line1\nline2\t\"quoted\"\\");
    json_end_object(&writer);
    read_temp_file(test_file);
    TEST_ASSERT(strstr(test_buffer, "\\n") != NULL, "AC-008: 换行符转义正确");
    TEST_ASSERT(strstr(test_buffer, "\\t") != NULL, "AC-008: 制表符转义正确");
    TEST_ASSERT(strstr(test_buffer, "\\\"") != NULL, "AC-008: 双引号转义正确");
    TEST_ASSERT(strstr(test_buffer, "\\\\") != NULL, "AC-008: 反斜杠转义正确");

    /* Test 6: Invalid arguments */
    printf("\n--- 测试 6: 无效参数处理 ---\n");
    ret = json_writer_init(NULL, test_file);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 写入器返回无效参数");
    ret = json_writer_init(&writer, NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 文件返回无效参数");
    ret = json_start_object(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 开始对象返回无效参数");
    ret = json_end_object(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 结束对象返回无效参数");
    ret = json_start_array(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 开始数组返回无效参数");
    ret = json_end_array(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 结束数组返回无效参数");
    ret = json_add_key(NULL, "key");
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 加键返回无效参数");
    ret = json_add_key(&writer, NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 键返回无效参数");
    ret = json_add_string(NULL, "value");
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 加字符串返回无效参数");
    ret = json_add_string(&writer, NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 字符串值返回无效参数");
    ret = json_add_int(NULL, 42);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 加整数返回无效参数");
    ret = json_add_bool(NULL, true);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 加布尔返回无效参数");
    ret = json_add_null(NULL);
    TEST_ASSERT(ret == LV_ERROR_INVALID_ARG, "AC-009: NULL 加 null 返回无效参数");

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
