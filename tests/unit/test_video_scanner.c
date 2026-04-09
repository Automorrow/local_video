#include "../../src/modules/video_scanner/video_scanner.h"
#include "../../src/modules/video_scanner/video_scanner_internal.h"
#include "../../src/modules/config/config.h"
#include "../../src/modules/db_manager/db_manager.h"
#include "../../src/include/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>

#ifndef _WIN32
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

static void test_helper_functions(void)
{
    printf("\n--- Test: Scanner helper functions ---\n");

    TEST_ASSERT(video_scanner_is_video_file("movie.mp4") == 1,
                "Recognizes mp4 files");
    TEST_ASSERT(video_scanner_is_video_file("movie.MKV") == 1,
                "Recognizes extensions case-insensitively");
    TEST_ASSERT(video_scanner_is_video_file("movie.txt") == 0,
                "Rejects unsupported extensions");
    TEST_ASSERT(video_scanner_is_video_file("movie") == 0,
                "Rejects files without extension");

    char title[256];
    video_scanner_extract_title("/tmp/example/test_video.mp4", title, sizeof(title));
    TEST_ASSERT(strcmp(title, "test_video") == 0,
                "Extracts title without extension");

    /* Test backslash path (Windows) */
    video_scanner_extract_title("C:\\Users\\test\\video.mp4", title, sizeof(title));
    TEST_ASSERT(strcmp(title, "video") == 0,
                "Extracts title from backslash path");

    char dir[256];
    video_scanner_extract_directory("/tmp/example/test_video.mp4", dir, sizeof(dir));
    TEST_ASSERT(strcmp(dir, "/tmp/example") == 0,
                "Extracts parent directory");

    /* Test backslash path (Windows) */
    video_scanner_extract_directory("C:\\Users\\test\\video.mp4", dir, sizeof(dir));
    TEST_ASSERT(strcmp(dir, "C:\\Users\\test") == 0,
                "Extracts parent directory from backslash path");

    video_scanner_extract_directory("filename_only.mp4", dir, sizeof(dir));
    TEST_ASSERT(strcmp(dir, "/") == 0,
                "Falls back to root for slashless path");
}

#ifdef _WIN32
/* ====== Windows-specific tests ====== */

static void test_watcher_lifecycle(void)
{
    printf("\n--- Test: Watcher lifecycle (Windows) ---\n");

    char temp_dir[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_dir);
    strcat(temp_dir, "lv_scanner_test_XXXXXX");

    /* Create unique temp dir */
    for (int i = 0; i < 99999; i++) {
        char candidate[MAX_PATH];
        snprintf(candidate, sizeof(candidate),
                 "%s\\lv_scanner_test_%05d", temp_dir, i);
        if (CreateDirectoryA(candidate, NULL)) {
            strcpy(temp_dir, candidate);
            break;
        }
    }

    char *argv[] = { "test_video_scanner", "--video-dir", temp_dir, "--db-path",
                     ":memory:" };
    config_parse_args(5, argv);

    lv_error_t err = db_manager_init(":memory:");
    TEST_ASSERT(err == LV_OK, "Initializes DB for watcher lifecycle test");
    if (err != LV_OK) {
        RemoveDirectoryA(temp_dir);
        return;
    }

    err = video_scanner_stop_watcher();
    TEST_ASSERT(err == LV_OK, "Stop watcher is idempotent before start");

    err = video_scanner_start_watcher();
    TEST_ASSERT(err == LV_OK, "Start watcher on temporary directory");

    err = video_scanner_start_watcher();
    TEST_ASSERT(err == LV_OK, "Second start is tolerated");

    err = video_scanner_stop_watcher();
    TEST_ASSERT(err == LV_OK, "Stop watcher after start");

    err = video_scanner_stop_watcher();
    TEST_ASSERT(err == LV_OK, "Second stop remains safe");

    db_manager_close();
    RemoveDirectoryA(temp_dir);
}

#else
/* ====== Linux-specific tests ====== */

static void test_watch_path_lookup(void)
{
    printf("\n--- Test: Watch path lookup ---\n");

    watch_dir_count = 2;
    watch_dirs[0].wd = 11;
    strcpy(watch_dirs[0].path, "/tmp/watch-a");
    watch_dirs[1].wd = 22;
    strcpy(watch_dirs[1].path, "/tmp/watch-b");

    TEST_ASSERT(strcmp(video_scanner_find_watch_path(11), "/tmp/watch-a") == 0,
                "Finds known watch descriptor path");
    TEST_ASSERT(strcmp(video_scanner_find_watch_path(99), "unknown") == 0,
                "Returns unknown for missing watch descriptor");

    watch_dir_count = 0;
}

static void test_watcher_lifecycle(void)
{
    printf("\n--- Test: Watcher lifecycle ---\n");

    char template[] = "/tmp/lv_scanner_test_XXXXXX";
    char *temp_dir = mkdtemp(template);
    TEST_ASSERT(temp_dir != NULL, "Creates temporary scan directory");
    if (!temp_dir) {
        return;
    }

    char *argv[] = { "test_video_scanner", "--video-dir", temp_dir, "--db-path",
                     ":memory:" };
    config_parse_args(5, argv);

    lv_error_t err = db_manager_init(":memory:");
    TEST_ASSERT(err == LV_OK, "Initializes DB for watcher lifecycle test");
    if (err != LV_OK) {
        rmdir(temp_dir);
        return;
    }

    err = video_scanner_stop_watcher();
    TEST_ASSERT(err == LV_OK, "Stop watcher is idempotent before start");

    err = video_scanner_start_watcher();
    TEST_ASSERT(err == LV_OK, "Start watcher on temporary directory");
    TEST_ASSERT(watcher_running == 1, "Watcher marked running after start");
    TEST_ASSERT(watcher_thread_started == 1, "Watcher thread marked started after start");

    err = video_scanner_start_watcher();
    TEST_ASSERT(err == LV_OK, "Second start is tolerated");

    err = video_scanner_stop_watcher();
    TEST_ASSERT(err == LV_OK, "Stop watcher after start");
    TEST_ASSERT(watcher_running == 0, "Watcher marked stopped after stop");
    TEST_ASSERT(watcher_thread_started == 0, "Watcher thread flag reset after stop");
    TEST_ASSERT(watch_dir_count == 0, "Watch registry cleared after stop");
    TEST_ASSERT(event_count == 0, "Pending event queue cleared after stop");

    err = video_scanner_stop_watcher();
    TEST_ASSERT(err == LV_OK, "Second stop remains safe");

    db_manager_close();
    rmdir(temp_dir);
}

static void create_nested_dirs(const char *root, int depth)
{
    char current[1024];
    snprintf(current, sizeof(current), "%s", root);

    for (int i = 0; i < depth; i++) {
        size_t len = strlen(current);
        snprintf(current + len, sizeof(current) - len, "/d%03d", i);
        mkdir(current, 0700);
    }
}

static void remove_nested_dirs(const char *root, int depth)
{
    char current[1024];
    snprintf(current, sizeof(current), "%s", root);

    for (int i = 0; i < depth; i++) {
        size_t len = strlen(current);
        snprintf(current + len, sizeof(current) - len, "/d%03d", i);
    }

    for (int i = depth - 1; i >= 0; i--) {
        rmdir(current);
        char *slash = strrchr(current, '/');
        if (!slash || strcmp(current, root) == 0) {
            break;
        }
        *slash = '\0';
    }
}

static void *start_watcher_thread(void *arg)
{
    lv_error_t *result = (lv_error_t *)arg;
    *result = video_scanner_start_watcher();
    return NULL;
}

static void test_stop_during_start(void)
{
    printf("\n--- Test: Stop during watcher start ---\n");

    char template[] = "/tmp/lv_scanner_race_XXXXXX";
    char *temp_dir = mkdtemp(template);
    TEST_ASSERT(temp_dir != NULL, "Creates temporary directory tree root");
    if (!temp_dir) {
        return;
    }

    create_nested_dirs(temp_dir, 200);

    char *argv[] = { "test_video_scanner", "--video-dir", temp_dir, "--db-path",
                     ":memory:" };
    config_parse_args(5, argv);

    lv_error_t err = db_manager_init(":memory:");
    TEST_ASSERT(err == LV_OK, "Initializes DB for stop-during-start test");
    if (err != LV_OK) {
        remove_nested_dirs(temp_dir, 200);
        rmdir(temp_dir);
        return;
    }

    pthread_t starter;
    lv_error_t start_result = LV_ERROR_UNKNOWN;
    int ret = pthread_create(&starter, NULL, start_watcher_thread, &start_result);
    TEST_ASSERT(ret == 0, "Creates background start thread");
    if (ret == 0) {
        usleep(1000);
        err = video_scanner_stop_watcher();
        TEST_ASSERT(err == LV_OK, "Stop watcher while start is in progress");
        pthread_join(starter, NULL);
        TEST_ASSERT(start_result == LV_OK, "Start returns cleanly after interruption");
    }

    TEST_ASSERT(watcher_running == 0, "Watcher not left running after interrupted start");
    TEST_ASSERT(watcher_thread_started == 0,
                "Watcher thread not marked started after interrupted start");
    TEST_ASSERT(inotify_fd == -1, "Inotify fd cleaned after interrupted start");
    TEST_ASSERT(epoll_fd == -1, "Epoll fd cleaned after interrupted start");
    TEST_ASSERT(watch_dir_count == 0, "Watch registry cleared after interrupted start");
    TEST_ASSERT(event_count == 0, "Pending queue cleared after interrupted start");

    db_manager_close();
    remove_nested_dirs(temp_dir, 200);
    rmdir(temp_dir);
}

#endif /* _WIN32 */

int main(void)
{
    printf("=== Video Scanner Module Unit Tests ===\n\n");

    test_helper_functions();

#ifndef _WIN32
    test_watch_path_lookup();
#endif
    test_watcher_lifecycle();

#ifndef _WIN32
    test_stop_during_start();
#endif

    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_count - test_results);
    printf("Failed: %d\n", test_results);

    if (test_results == 0) {
        printf("\nAll tests passed!\n");
        return EXIT_SUCCESS;
    }

    printf("\nSome tests failed!\n");
    return EXIT_FAILURE;
}
