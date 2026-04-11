#include "db_manager.h"
#include "local_video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define TEST_INIT() \
    do { \
        test_results = 0; \
        test_count = 0; \
    } while (0)

static int video_count = 0;
static int video_last_id = 0;

static int video_callback(const VideoInfo *video, void *user_data) {
    if (user_data) {
        int *count = (int *)user_data;
        (*count)++;
    }
    video_last_id = (int)video->id;
    return 0;
}

static int history_callback(const HistoryInfo *history, void *user_data) {
    (void)history;
    if (user_data) {
        int *count = (int *)user_data;
        (*count)++;
    }
    return 0;
}

static int favorite_callback(const FavoriteInfo *favorite, void *user_data) {
    (void)favorite;
    if (user_data) {
        int *count = (int *)user_data;
        (*count)++;
    }
    return 0;
}

static void test_init_close(void) {
    printf("\n--- Test: Init/Close ---\n");
    
    lv_error_t err = db_manager_init(":memory:");
    TEST_ASSERT(err == LV_OK, "DB init with memory database");
    
    err = db_manager_close();
    TEST_ASSERT(err == LV_OK, "DB close");
    
    err = db_manager_init(NULL);
    TEST_ASSERT(err == LV_ERROR_INVALID_ARG, "DB init with NULL path fails");
}

static void test_video_crud(void) {
    printf("\n--- Test: Video CRUD ---\n");
    
    lv_error_t err = db_manager_init(":memory:");
    TEST_ASSERT(err == LV_OK, "DB init for video tests");
    
    err = db_manager_video_insert("/path/to/video1.mp4", "Test Video 1", "Test", 1024000);
    TEST_ASSERT(err == LV_OK, "Video insert");
    
    err = db_manager_video_insert("/path/to/video2.mp4", "Test Video 2", "Test", 2048000);
    TEST_ASSERT(err == LV_OK, "Video insert second");
    
    err = db_manager_video_insert("/path/to/video3.mp4", "Another Video", "Other", 3072000);
    TEST_ASSERT(err == LV_OK, "Video insert third (different category)");
    
    video_count = 0;
    err = db_manager_video_get_all(video_callback, &video_count);
    TEST_ASSERT(err == LV_OK, "Video get all");
    TEST_ASSERT(video_count == 3, "Video get all returns 3 videos");
    
    VideoInfo info;
    err = db_manager_video_get_by_id(video_last_id, &info);
    TEST_ASSERT(err == LV_OK, "Video get by id");
    
    video_count = 0;
    err = db_manager_video_search("Test", video_callback, &video_count);
    TEST_ASSERT(err == LV_OK, "Video search");
    TEST_ASSERT(video_count == 2, "Video search returns 2 results");
    
    video_count = 0;
    err = db_manager_video_search("Another", video_callback, &video_count);
    TEST_ASSERT(err == LV_OK, "Video search Another");
    TEST_ASSERT(video_count == 1, "Video search returns 1 result");
    
    video_count = 0;
    err = db_manager_video_get_by_category("Test", video_callback, &video_count);
    TEST_ASSERT(err == LV_OK, "Video get by category");
    TEST_ASSERT(video_count == 2, "Video get by category returns 2");
    
    video_count = 0;
    err = db_manager_video_get_by_category("Other", video_callback, &video_count);
    TEST_ASSERT(err == LV_OK, "Video get by category Other");
    TEST_ASSERT(video_count == 1, "Video get by category returns 1");
    
    err = db_manager_video_delete(video_last_id);
    TEST_ASSERT(err == LV_OK, "Video delete");
    
    video_count = 0;
    err = db_manager_video_get_all(video_callback, &video_count);
    TEST_ASSERT(err == LV_OK, "Video get all after delete");
    TEST_ASSERT(video_count == 2, "Video get all returns 2 after delete");
    
    db_manager_close();
}

static void test_history_crud(void) {
    printf("\n--- Test: History CRUD ---\n");

    lv_error_t err = db_manager_init(":memory:");
    TEST_ASSERT(err == LV_OK, "DB init for history tests");

    err = db_manager_video_insert("/path/to/video1.mp4", "Test Video 1", "Test", 1024000);
    TEST_ASSERT(err == LV_OK, "Insert video for history test");
    err = db_manager_video_insert("/path/to/video2.mp4", "Test Video 2", "Test", 1024000);
    TEST_ASSERT(err == LV_OK, "Insert second video for history test");

    int history_count = 0;
    err = db_manager_history_add(1, 100);
    TEST_ASSERT(err == LV_OK, "History add");

    /* Same video consecutive play updates the existing record */
    err = db_manager_history_add(1, 200);
    TEST_ASSERT(err == LV_OK, "History add second (same video updates)");

    /* Different video inserts a new record */
    err = db_manager_history_add(2, 300);
    TEST_ASSERT(err == LV_OK, "History add third (different video)");

    err = db_manager_history_get(history_callback, &history_count);
    TEST_ASSERT(err == LV_OK, "History get");
    TEST_ASSERT(history_count == 2, "History get returns 2");

    err = db_manager_history_delete(1);
    TEST_ASSERT(err == LV_OK, "History delete");

    history_count = 0;
    err = db_manager_history_get(history_callback, &history_count);
    TEST_ASSERT(err == LV_OK, "History get after delete");
    TEST_ASSERT(history_count == 1, "History get returns 1 after delete");

    err = db_manager_history_clear();
    TEST_ASSERT(err == LV_OK, "History clear");

    history_count = 0;
    err = db_manager_history_get(history_callback, &history_count);
    TEST_ASSERT(err == LV_OK, "History get after clear");
    TEST_ASSERT(history_count == 0, "History get returns 0 after clear");

    db_manager_close();
}

static void test_favorites_crud(void) {
    printf("\n--- Test: Favorites CRUD ---\n");
    
    lv_error_t err = db_manager_init(":memory:");
    TEST_ASSERT(err == LV_OK, "DB init for favorites tests");
    
    err = db_manager_video_insert("/path/to/video1.mp4", "Test Video 1", "Test", 1024000);
    TEST_ASSERT(err == LV_OK, "Insert video for favorites test");
    
    err = db_manager_video_insert("/path/to/video2.mp4", "Test Video 2", "Test", 2048000);
    TEST_ASSERT(err == LV_OK, "Insert second video for favorites test");
    
    err = db_manager_favorite_add(1);
    TEST_ASSERT(err == LV_OK, "Favorite add");
    
    err = db_manager_favorite_add(2);
    TEST_ASSERT(err == LV_OK, "Favorite add second");
    
    int fav_count = 0;
    err = db_manager_favorites_list(favorite_callback, &fav_count);
    TEST_ASSERT(err == LV_OK, "Favorites list");
    TEST_ASSERT(fav_count == 2, "Favorites list returns 2");
    
    bool is_fav = false;
    err = db_manager_favorite_check(1, &is_fav);
    TEST_ASSERT(err == LV_OK, "Favorite check");
    TEST_ASSERT(is_fav == true, "Video 1 is favorite");
    
    is_fav = false;
    err = db_manager_favorite_check(999, &is_fav);
    TEST_ASSERT(err == LV_OK, "Favorite check non-existent");
    TEST_ASSERT(is_fav == false, "Video 999 is not favorite");
    
    err = db_manager_favorite_remove(1);
    TEST_ASSERT(err == LV_OK, "Favorite remove");
    
    fav_count = 0;
    err = db_manager_favorites_list(favorite_callback, &fav_count);
    TEST_ASSERT(err == LV_OK, "Favorites list after remove");
    TEST_ASSERT(fav_count == 1, "Favorites list returns 1 after remove");
    
    db_manager_close();
}

static void test_random(void) {
    printf("\n--- Test: Random Selection ---\n");
    
    lv_error_t err = db_manager_init(":memory:");
    TEST_ASSERT(err == LV_OK, "DB init for random tests");
    
    for (int i = 0; i < 10; i++) {
        char path[128];
        char title[128];
        snprintf(path, sizeof(path), "/path/to/video%d.mp4", i);
        snprintf(title, sizeof(title), "Video %d", i);
        err = db_manager_video_insert(path, title, "Random", 1000000 + i);
        TEST_ASSERT(err == LV_OK, "Insert video for random test");
    }
    
    video_count = 0;
    err = db_manager_video_get_random(3, video_callback, &video_count);
    TEST_ASSERT(err == LV_OK, "Random get 3 videos");
    TEST_ASSERT(video_count == 3, "Random returns 3 videos");
    
    video_count = 0;
    err = db_manager_video_get_random(5, video_callback, &video_count);
    TEST_ASSERT(err == LV_OK, "Random get 5 videos");
    TEST_ASSERT(video_count == 5, "Random returns 5 videos");
    
    video_count = 0;
    err = db_manager_video_get_random(10, video_callback, &video_count);
    TEST_ASSERT(err == LV_OK, "Random get 10 videos");
    TEST_ASSERT(video_count == 10, "Random returns 10 videos");
    
    db_manager_close();
}

int main(void) {
    printf("=== DB Manager Module Unit Tests ===\n\n");
    
    test_init_close();
    test_video_crud();
    test_history_crud();
    test_favorites_crud();
    test_random();
    
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_count - test_results);
    printf("Failed: %d\n", test_results);
    
    if (test_results == 0) {
        printf("\nAll tests passed!\n");
        return EXIT_SUCCESS;
    } else {
        printf("\nSome tests failed!\n");
        return EXIT_FAILURE;
    }
}
