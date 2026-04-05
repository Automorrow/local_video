#include "video_scanner.h"
#include "db_manager.h"
#include "module.h"
#include "log.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static volatile int watcher_running = 0;
static int inotify_fd = -1;
static int epoll_fd = -1;
static pthread_t watcher_thread;

#define MAX_WATCH_DIRS 2048
static struct {
    int wd;
    char path[1024];
} watch_dirs[MAX_WATCH_DIRS];
static int watch_dir_count = 0;

#define DEBOUNCE_MS 500
#define BATCH_SIZE 64
#define MAX_EPOLL_EVENTS 64

typedef struct {
    char path[1024];
    uint32_t mask;
    struct timespec timestamp;
} pending_event_t;

static pending_event_t event_queue[BATCH_SIZE];
static int event_count = 0;
static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Video file extensions */
static const char *video_extensions[] = {
    ".mp4", ".webm", ".mkv", ".avi", ".mov", ".wmv", ".flv", NULL
};

static int is_video_file(const char *filename)
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

static void extract_title(const char *path, char *title, size_t title_size)
{
    const char *filename = strrchr(path, '/');
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

static void extract_directory(const char *full_path, char *dir, size_t dir_size)
{
    const char *last_slash = strrchr(full_path, '/');
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

static const char *find_watch_path(int wd)
{
    for (int i = 0; i < watch_dir_count; i++) {
        if (watch_dirs[i].wd == wd) {
            return watch_dirs[i].path;
        }
    }
    return "unknown";
}

/* ====== Directory Watcher Registration ====== */

static int watch_directory_recursive(const char *path)
{
    if (inotify_fd < 0 || !path) return -1;

    bool in_blacklist = false;
    if (db_manager_blacklist_check(path, &in_blacklist) == LV_OK && in_blacklist) {
        log_debug("Skipping blacklisted directory watch: %s", path);
        return -1;
    }

    int wd = inotify_add_watch(inotify_fd, path,
        IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_MODIFY |
        IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR);

    if (wd < 0) {
        log_warning("Failed to add watch: %s (%s)", path, strerror(errno));
        return -1;
    }

    if (watch_dir_count < MAX_WATCH_DIRS) {
        watch_dirs[watch_dir_count].wd = wd;
        strncpy(watch_dirs[watch_dir_count].path, path,
                sizeof(watch_dirs[0].path) - 1);
        watch_dir_count++;
    }

    log_debug("Watching directory: %s (wd=%d, total=%d)", path, wd, watch_dir_count);

    DIR *dir = opendir(path);
    if (!dir) return wd;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            watch_directory_recursive(full_path);
        }
    }

    closedir(dir);
    return wd;
}

/* ====== Event Processing ====== */

static void process_video_create(const char *real_path)
{
    struct stat st;
    if (stat(real_path, &st) < 0 || st.st_size < 1024 * 1024) return;

    bool in_blacklist = false;
    if (db_manager_blacklist_check(real_path, &in_blacklist) == LV_OK && in_blacklist) return;

    char title[256], category[512];
    extract_title(real_path, title, sizeof(title));
    extract_directory(real_path, category, sizeof(category));

    lv_error_t err = db_manager_video_insert(real_path, title, category, st.st_size);
    if (err == LV_OK) {
        log_info("[增量] 新视频: %s (%ld bytes)", title, (long)st.st_size);
    }
}

static void process_video_delete(const char *real_path)
{
    lv_error_t err = db_manager_video_delete_by_path(real_path);
    if (err == LV_OK) {
        const char *filename = strrchr(real_path, '/');
        log_info("[增量] 视频已删除: %s", filename ? filename + 1 : real_path);
    }
}

static void process_video_modify(const char *real_path)
{
    struct stat st;
    if (stat(real_path, &st) < 0) return;

    char title[256], category[512];
    extract_title(real_path, title, sizeof(title));
    extract_directory(real_path, category, sizeof(category));

    lv_error_t err = db_manager_video_update(real_path, title, category, st.st_size);
    if (err == LV_OK) {
        log_debug("[增量] 视频已更新: %s", title);
    }
}

static void handle_single_event(const char *path, uint32_t mask)
{
    if (!is_video_file(path)) return;

    char real_path[PATH_MAX];
    if (realpath(path, real_path) == NULL) {
        log_debug("[增量] 无法解析路径: %s", path);
        return;
    }

    if (mask & (IN_DELETE | IN_MOVED_FROM)) {
        process_video_delete(real_path);
    } else if (mask & (IN_CREATE | IN_MOVED_TO)) {
        process_video_create(real_path);
    } else if (mask & IN_MODIFY) {
        process_video_modify(real_path);
    }
}

static void flush_event_batch(void)
{
    pthread_mutex_lock(&event_mutex);

    for (int i = 0; i < event_count; i++) {
        handle_single_event(event_queue[i].path, event_queue[i].mask);
    }
    event_count = 0;

    pthread_mutex_unlock(&event_mutex);
}

static void enqueue_event(const char *path, uint32_t mask)
{
    pthread_mutex_lock(&event_mutex);

    if (event_count < BATCH_SIZE) {
        size_t path_len = strlen(path);
        if (path_len >= sizeof(event_queue[event_count].path)) {
            path_len = sizeof(event_queue[event_count].path) - 1;
        }
        memcpy(event_queue[event_count].path, path, path_len);
        event_queue[event_count].path[path_len] = '\0';
        event_queue[event_count].mask = mask;
        clock_gettime(CLOCK_MONOTONIC, &event_queue[event_count].timestamp);
        event_count++;
    } else {
        log_warning("[增量] 事件队列已满，立即刷新");
        pthread_mutex_unlock(&event_mutex);
        flush_event_batch();
        pthread_mutex_lock(&event_mutex);
        if (event_count < BATCH_SIZE) {
            size_t path_len = strlen(path);
            if (path_len >= sizeof(event_queue[event_count].path)) {
                path_len = sizeof(event_queue[event_count].path) - 1;
            }
            memcpy(event_queue[event_count].path, path, path_len);
            event_queue[event_count].path[path_len] = '\0';
            event_queue[event_count].mask = mask;
            clock_gettime(CLOCK_MONOTONIC, &event_queue[event_count].timestamp);
            event_count++;
        }
    }

    pthread_mutex_unlock(&event_mutex);
}

static void process_inotify_event(struct inotify_event *event)
{
    if (!event->len || !event->name) return;

    const char *watch_root = find_watch_path(event->wd);

    if (event->mask & IN_ISDIR) {
        if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
            char new_dir[1024];
            snprintf(new_dir, sizeof(new_dir), "%s/%s", watch_root, event->name);
            log_info("[增量] 新子目录: %s", new_dir);
            watch_directory_recursive(new_dir);
        } else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
            log_info("[增量] 子目录删除/移出: %s/%s", watch_root, event->name);
        }
    } else {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", watch_root, event->name);
        enqueue_event(path, event->mask);
    }
}

/* ====== Watcher Thread (epoll loop) ====== */

static void *watcher_thread_func(void *arg)
{
    (void)arg;

    struct epoll_event events[MAX_EPOLL_EVENTS];
    char buffer[1024 * (sizeof(struct inotify_event) + 256)];

    log_info("[增量监控] 文件监控线程启动 (inotify + epoll)");

    while (watcher_running) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, DEBOUNCE_MS);

        if (nfds < 0) {
            if (errno == EINTR) continue;
            log_error("[增量监控] epoll_wait 错误: %s", strerror(errno));
            break;
        }

        if (nfds > 0) {
            for (int i = 0; i < nfds; i++) {
                if (events[i].data.fd != inotify_fd) continue;

                ssize_t len = read(inotify_fd, buffer, sizeof(buffer));
                if (len < 0) {
                    if (errno == EAGAIN) continue;
                    log_error("[增量监控] read inotify 错误: %s", strerror(errno));
                    break;
                }

                char *ptr = buffer;
                while (ptr < buffer + len) {
                    struct inotify_event *event = (struct inotify_event *)ptr;
                    process_inotify_event(event);
                    ptr += sizeof(struct inotify_event) + event->len;
                }
            }
        }

        flush_event_batch();
    }

    flush_event_batch();
    log_info("[增量监控] 文件监控线程停止");
    return NULL;
}

/* ====== Public API ====== */

lv_error_t video_scanner_start_watcher(void)
{
    if (watcher_running) {
        log_warning("[增量监控] 监控已在运行");
        return LV_OK;
    }

    const lv_config_t *config = config_get();
    if (!config || !config->scan_directory) {
        log_error("[增量监控] 未配置扫描目录");
        return LV_ERROR_INVALID_ARG;
    }

    inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd < 0) {
        log_error("[增量监控] 初始化 inotify 失败: %s", strerror(errno));
        return LV_ERROR_UNKNOWN;
    }

    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        log_error("[增量监控] 创建 epoll 失败: %s", strerror(errno));
        close(inotify_fd);
        inotify_fd = -1;
        return LV_ERROR_UNKNOWN;
    }

    struct epoll_event ev = { .events = EPOLLIN, .data.fd = inotify_fd };
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inotify_fd, &ev) < 0) {
        log_error("[增量监控] 注册 inotify 到 epoll 失败: %s", strerror(errno));
        close(epoll_fd);
        close(inotify_fd);
        epoll_fd = -1;
        inotify_fd = -1;
        return LV_ERROR_UNKNOWN;
    }

    log_info("[增量监控] 注册目录监控: %s", config->scan_directory);
    watch_directory_recursive(config->scan_directory);
    log_info("[增量监控] 已注册 %d 个目录监控点", watch_dir_count);

    watcher_running = 1;
    if (pthread_create(&watcher_thread, NULL, watcher_thread_func, NULL) != 0) {
        log_error("[增量监控] 创建监控线程失败");
        close(epoll_fd);
        close(inotify_fd);
        epoll_fd = -1;
        inotify_fd = -1;
        watcher_running = 0;
        return LV_ERROR_UNKNOWN;
    }

    log_info("[增量监控] 增量文件监控系统初始化完成");
    return LV_OK;
}

lv_error_t video_scanner_stop_watcher(void)
{
    if (!watcher_running) return LV_OK;

    log_info("[增量监控] 正在停止文件监控...");
    watcher_running = 0;

    if (inotify_fd >= 0) {
        close(inotify_fd);
        inotify_fd = -1;
    }

    if (epoll_fd >= 0) {
        close(epoll_fd);
        epoll_fd = -1;
    }

    pthread_join(watcher_thread, NULL);
    watch_dir_count = 0;
    log_info("[增量监控] 文件监控已停止");
    return LV_OK;
}

static volatile int scan_in_progress = 0;

static lv_error_t scan_single_file(const char *full_path)
{
    struct stat st;
    if (stat(full_path, &st) < 0) return LV_ERROR_IO;
    if (!S_ISREG(st.st_mode)) return LV_OK;
    if (!is_video_file(full_path)) return LV_OK;
    if (st.st_size < 1024 * 1024) return LV_OK;

    char real_path[PATH_MAX];
    if (realpath(full_path, real_path) == NULL) return LV_ERROR_IO;

    bool in_blacklist = false;
    if (db_manager_blacklist_check(real_path, &in_blacklist) == LV_OK && in_blacklist) {
        return LV_OK;
    }

    char title[256], category[512];
    extract_title(real_path, title, sizeof(title));
    extract_directory(real_path, category, sizeof(category));

    return db_manager_video_insert(real_path, title, category, st.st_size);
}

static lv_error_t scan_directory_incremental(const char *dir_path, int *file_count)
{
    bool in_blacklist = false;
    if (db_manager_blacklist_check(dir_path, &in_blacklist) == LV_OK && in_blacklist) {
        return LV_OK;
    }

    DIR *dir = opendir(dir_path);
    if (!dir) return LV_ERROR_IO;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) < 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_directory_incremental(full_path, file_count);
        } else {
            if (scan_single_file(full_path) == LV_OK) {
                (*file_count)++;
            }
        }
    }

    closedir(dir);
    return LV_OK;
}

static void *scan_thread_func(void *arg)
{
    const char *directory = (const char *)arg;

    const lv_config_t *config = config_get();
    const char *scan_dir = directory ? directory : (config ? config->scan_directory : NULL);

    if (!scan_dir) {
        log_error("[扫描] 未指定扫描目录");
        scan_in_progress = 0;
        return NULL;
    }

    log_info("[扫描] 开始扫描目录: %s", scan_dir);

    int file_count = 0;
    scan_directory_incremental(scan_dir, &file_count);

    log_info("[扫描] 扫描完成: 发现/添加 %d 个视频文件", file_count);
    scan_in_progress = 0;
    return NULL;
}

lv_error_t video_scanner_scan(const char *directory)
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

lv_error_t video_scanner_rescan(void)
{
    return video_scanner_scan(config_get()->scan_directory);
}

/* ====== Module Lifecycle ====== */

static void video_scanner_init(void)
{
    log_info("[模块] Video scanner 初始化");
    int64_t video_count = 0;
    db_manager_video_count(&video_count);
    if (video_count == 0) {
        log_info("[扫描] 数据库为空，开始初始扫描: %s", config_get()->scan_directory);
        video_scanner_scan(config_get()->scan_directory);
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
