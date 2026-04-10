#include "video_scanner_internal.h"
#include "../config/config.h"
#include "../db_manager/db_manager.h"
#include "../../shared/log/log.h"
#include "../../include/platform.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_SCAN_DEPTH 128

#ifdef _WIN32
/* ====== Windows Implementation (FindFirstFileW/FindNextFileW) ====== */
#include <windows.h>
#include <io.h>

/* Forward declarations */
static lv_error_t scan_directory_incremental_ex(const char *dir_path, int *file_count, int depth);

static lv_error_t scan_single_file(const char *full_path)
{
    struct _stat64 st;
    if (_stat64(full_path, &st) < 0) return LV_ERROR_IO;
    if (!S_ISREG(st.st_mode)) return LV_OK;
    if (!video_scanner_is_video_file(full_path)) return LV_OK;
    if (st.st_size < 1024 * 1024) return LV_OK;

    char real_path[4096];
    if (_fullpath(real_path, full_path, sizeof(real_path)) == NULL) return LV_ERROR_IO;

    /* Normalize path separators to forward slash for consistency */
    for (char *p = real_path; *p; p++) {
        if (*p == '\\') *p = '/';
    }

    bool in_blacklist = false;
    if (db_manager_blacklist_check(real_path, &in_blacklist) == LV_OK && in_blacklist) {
        return LV_OK;
    }

    char title[256];
    char category[512];
    video_scanner_extract_title(real_path, title, sizeof(title));
    video_scanner_extract_directory(real_path, category, sizeof(category));

    return db_manager_video_insert(real_path, title, category, st.st_size);
}

static lv_error_t scan_directory_incremental(const char *dir_path, int *file_count)
{
    return scan_directory_incremental_ex(dir_path, file_count, 0);
}

static lv_error_t scan_directory_incremental_ex(const char *dir_path, int *file_count, int depth)
{
    if (depth >= MAX_SCAN_DEPTH) {
        log_warning("[扫描] 目录嵌套过深，跳过: %s", dir_path);
        return LV_OK;
    }

    bool in_blacklist = false;
    if (db_manager_blacklist_check(dir_path, &in_blacklist) == LV_OK && in_blacklist) {
        return LV_OK;
    }

    char search_path[4096];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATAW find_data;

    /* Use wide-char for proper Unicode support */
    WCHAR wsearch_path[4096];
    MultiByteToWideChar(CP_UTF8, 0, search_path, -1, wsearch_path, 4096);

    hFind = FindFirstFileW(wsearch_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return LV_ERROR_IO;

    do {
        /* Skip . and .. */
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }

        /* Convert wide filename to UTF-8 */
        char utf8_name[1024];
        WideCharToMultiByte(CP_UTF8, 0, find_data.cFileName, -1,
                           utf8_name, sizeof(utf8_name), NULL, NULL);

        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, utf8_name);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* Skip hidden directories and reparse points (symlinks) */
            if (find_data.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_REPARSE_POINT)) {
                continue;
            }
            scan_directory_incremental_ex(full_path, file_count, depth + 1);
        } else {
            if (scan_single_file(full_path) == LV_OK) {
                (*file_count)++;
            }
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);
    return LV_OK;
}

#else /* POSIX / Linux */
/* ====== Linux Implementation (opendir/readdir) ====== */
#include <dirent.h>
#include <limits.h>
#include <unistd.h>

/* Forward declarations */
static lv_error_t scan_directory_incremental_ex(const char *dir_path, int *file_count, int depth);

static lv_error_t scan_single_file(const char *full_path)
{
    struct stat st;
    if (stat(full_path, &st) < 0) return LV_ERROR_IO;
    if (!S_ISREG(st.st_mode)) return LV_OK;
    if (!video_scanner_is_video_file(full_path)) return LV_OK;
    if (st.st_size < 1024 * 1024) return LV_OK;

    char real_path[PATH_MAX];
    if (realpath(full_path, real_path) == NULL) return LV_ERROR_IO;

    bool in_blacklist = false;
    if (db_manager_blacklist_check(real_path, &in_blacklist) == LV_OK && in_blacklist) {
        return LV_OK;
    }

    char title[256];
    char category[512];
    video_scanner_extract_title(real_path, title, sizeof(title));
    video_scanner_extract_directory(real_path, category, sizeof(category));

    return db_manager_video_insert(real_path, title, category, st.st_size);
}

static lv_error_t scan_directory_incremental(const char *dir_path, int *file_count)
{
    return scan_directory_incremental_ex(dir_path, file_count, 0);
}

static lv_error_t scan_directory_incremental_ex(const char *dir_path, int *file_count, int depth)
{
    if (depth >= MAX_SCAN_DEPTH) {
        log_warning("[扫描] 目录嵌套过深，跳过: %s", dir_path);
        return LV_OK;
    }

    bool in_blacklist = false;
    if (db_manager_blacklist_check(dir_path, &in_blacklist) == LV_OK && in_blacklist) {
        return LV_OK;
    }

    DIR *dir = opendir(dir_path);
    if (!dir) return LV_ERROR_IO;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) < 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_directory_incremental_ex(full_path, file_count, depth + 1);
        } else {
            if (scan_single_file(full_path) == LV_OK) {
                (*file_count)++;
            }
        }
    }

    closedir(dir);
    return LV_OK;
}

#endif /* _WIN32 */

/* ====== Platform-independent scan thread ====== */

static void *scan_thread_func(void *arg)
{
    char *directory_copy = (char *)arg;
    const lv_config_t *config = config_get();
    const char *scan_dir = directory_copy ? directory_copy :
        (config ? config->scan_directory : NULL);

    if (!scan_dir) {
        log_error("[扫描] 未指定扫描目录");
        free(directory_copy);
        scan_in_progress = 0;
        return NULL;
    }

    log_info("[扫描] 开始扫描目录: %s", scan_dir);

    int file_count = 0;
    scan_directory_incremental(scan_dir, &file_count);

    log_info("[扫描] 扫描完成: 发现/添加 %d 个视频文件", file_count);
    free(directory_copy);
    scan_in_progress = 0;
    return NULL;
}

lv_error_t video_scanner_scan_impl(const char *directory)
{
    if (scan_in_progress) {
        log_warning("[扫描] 扫描已在进行中");
        return LV_OK;
    }

    scan_in_progress = 1;

    char *dir_copy = directory ? strdup(directory) : NULL;
    pthread_t thread;
    int ret = pthread_create(&thread, NULL, scan_thread_func, dir_copy);
    if (ret != 0) {
        log_error("[扫描] 创建扫描线程失败");
        free(dir_copy);
        scan_in_progress = 0;
        return LV_ERROR_UNKNOWN;
    }

    pthread_detach(thread);
    return LV_OK;
}
