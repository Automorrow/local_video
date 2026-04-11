#include "video_scanner_internal.h"
#include "../config/config.h"
#include "../db_manager/db_manager.h"
#include "../../shared/module/module.h"
#include "../../shared/log/log.h"
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef _WIN32
/* Linux-only global state (inotify/epoll) */
volatile int watcher_running = 0;
int watcher_thread_started = 0;
int inotify_fd = -1;
int epoll_fd = -1;
pthread_t watcher_thread;
watch_dir_entry_t watch_dirs[MAX_WATCH_DIRS];
int watch_dir_count = 0;
pending_event_t event_queue[BATCH_SIZE];
int event_count = 0;
pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

volatile int scan_in_progress = 0;

static const char *video_extensions[] = {
    ".mp4", ".webm", ".mkv", ".avi", ".mov", ".wmv", ".flv", NULL
};

int video_scanner_is_video_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;

    ext++;
    for (int i = 0; video_extensions[i]; i++) {
        if (strcasecmp(ext, video_extensions[i] + 1) == 0) {
            return 1;
        }
    }
    return 0;
}

void video_scanner_extract_title(const char *path, char *title, size_t title_size)
{
    const char *filename = strrchr(path, '/');
    if (!filename) {
        /* Also try backslash for Windows paths */
        filename = strrchr(path, '\\');
    }
    if (!filename) filename = path;
    else filename++;

    const char *ext = strrchr(filename, '.');
    size_t len;
    if (ext) {
        len = (size_t)(ext - filename);
    } else {
        len = strlen(filename);
    }

    if (len >= title_size) len = title_size - 1;
    memcpy(title, filename, len);
    title[len] = '\0';
}

void video_scanner_extract_directory(const char *full_path, char *dir,
                                     size_t dir_size)
{
    const char *last_slash = strrchr(full_path, '/');
    if (!last_slash) {
        last_slash = strrchr(full_path, '\\');
    }
    if (!last_slash) {
        dir[0] = '/';
        dir[1] = '\0';
        return;
    }

    size_t len = (size_t)(last_slash - full_path);
    if (len >= dir_size) len = dir_size - 1;
    memcpy(dir, full_path, len);
    dir[len] = '\0';
}

#ifndef _WIN32
const char *video_scanner_find_watch_path(int wd)
{
    for (int i = 0; i < watch_dir_count; i++) {
        if (watch_dirs[i].wd == wd) {
            return watch_dirs[i].path;
        }
    }
    return "unknown";
}
#endif

lv_error_t video_scanner_start_watcher(void)
{
    return video_scanner_start_watcher_impl();
}

lv_error_t video_scanner_stop_watcher(void)
{
    return video_scanner_stop_watcher_impl();
}

lv_error_t video_scanner_scan(const char *directory)
{
    return video_scanner_scan_impl(directory);
}

lv_error_t video_scanner_rescan(void)
{
    return video_scanner_scan(config_get()->scan_directory);
}

/* ====== Module Lifecycle ====== */

static void video_scanner_init(void)
{
    log_info("[模块] Video scanner 初始化");

    /* Ensure scan directory exists */
    const char *scan_dir = config_get()->scan_directory;
#ifdef _WIN32
    CreateDirectoryA(scan_dir, NULL);
#else
    mkdir(scan_dir, 0755);
#endif

    int64_t video_count = 0;
    db_manager_video_count(&video_count);
    if (video_count == 0) {
        log_info("[扫描] 数据库为空，开始初始扫描: %s", scan_dir);
        video_scanner_scan(scan_dir);
    }
}

static void video_scanner_sub(void)
{
    log_info("[模块] Video scanner 已订阅事件");
}

static void video_scanner_run(void)
{
    log_info("[模块] 启动增量文件监控系统...");
    video_scanner_start_watcher();
}

static void video_scanner_exit(void)
{
    log_info("[模块] 停止 Video scanner...");
    video_scanner_stop_watcher();
    log_info("[模块] Video scanner 已退出");
}

MODULE_INIT(video_scanner_init, "video_scanner");
MODULE_SUB(video_scanner_sub, "video_scanner");
MODULE_RUN(video_scanner_run, "video_scanner");
MODULE_EXIT(video_scanner_exit, "video_scanner");
