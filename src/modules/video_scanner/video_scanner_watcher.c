#include "video_scanner_internal.h"
#include "../db_manager/db_manager.h"
#include "../config/config.h"
#include "../../shared/log/log.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

static void reset_watcher_startup_state(void)
{
    if (epoll_fd >= 0) {
        close(epoll_fd);
        epoll_fd = -1;
    }

    if (inotify_fd >= 0) {
        close(inotify_fd);
        inotify_fd = -1;
    }

    watch_dir_count = 0;
    event_count = 0;
    watcher_thread_started = 0;
}

static int watch_directory_recursive(const char *path)
{
    if (inotify_fd < 0 || !path) return -1;
    if (!watcher_running) return -1;

    bool in_blacklist = false;
    if (db_manager_blacklist_check(path, &in_blacklist) == LV_OK && in_blacklist) {
        log_debug("Skipping blacklisted directory watch: %s", path);
        return -1;
    }

    int wd = inotify_add_watch(inotify_fd, path,
        IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_MODIFY |
        IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR);

    if (wd < 0) {
        if (!watcher_running || inotify_fd < 0 || errno == EBADF) {
            return -1;
        }
        log_warning("Failed to add watch: %s (%s)", path, strerror(errno));
        return -1;
    }

    if (watch_dir_count < MAX_WATCH_DIRS) {
        watch_dirs[watch_dir_count].wd = wd;
        strncpy(watch_dirs[watch_dir_count].path, path,
                sizeof(watch_dirs[0].path) - 1);
        watch_dirs[watch_dir_count].path[sizeof(watch_dirs[0].path) - 1] = '\0';
        watch_dir_count++;
    }

    log_debug("Watching directory: %s (wd=%d, total=%d)", path, wd,
              watch_dir_count);

    DIR *dir = opendir(path);
    if (!dir) return wd;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!watcher_running) {
            break;
        }

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

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

static void process_video_create(const char *real_path)
{
    struct stat st;
    if (stat(real_path, &st) < 0 || st.st_size < 1024 * 1024) return;

    bool in_blacklist = false;
    if (db_manager_blacklist_check(real_path, &in_blacklist) == LV_OK && in_blacklist) {
        return;
    }

    char title[256];
    char category[512];
    video_scanner_extract_title(real_path, title, sizeof(title));
    video_scanner_extract_directory(real_path, category, sizeof(category));

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

    char title[256];
    char category[512];
    video_scanner_extract_title(real_path, title, sizeof(title));
    video_scanner_extract_directory(real_path, category, sizeof(category));

    lv_error_t err = db_manager_video_update(real_path, title, category, st.st_size);
    if (err == LV_OK) {
        log_debug("[增量] 视频已更新: %s", title);
    }
}

static void handle_single_event(const char *path, uint32_t mask)
{
    if (!video_scanner_is_video_file(path)) return;

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
    if (!event->len) return;

    const char *watch_root = video_scanner_find_watch_path(event->wd);

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

lv_error_t video_scanner_start_watcher_impl(void)
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

    watch_dir_count = 0;
    event_count = 0;
    watcher_thread_started = 0;
    watcher_running = 1;

    inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd < 0) {
        log_error("[增量监控] 初始化 inotify 失败: %s", strerror(errno));
        watcher_running = 0;
        return LV_ERROR_UNKNOWN;
    }

    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        log_error("[增量监控] 创建 epoll 失败: %s", strerror(errno));
        reset_watcher_startup_state();
        watcher_running = 0;
        return LV_ERROR_UNKNOWN;
    }

    struct epoll_event ev = { .events = EPOLLIN, .data.fd = inotify_fd };
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inotify_fd, &ev) < 0) {
        log_error("[增量监控] 注册 inotify 到 epoll 失败: %s", strerror(errno));
        reset_watcher_startup_state();
        watcher_running = 0;
        return LV_ERROR_UNKNOWN;
    }

    log_info("[增量监控] 注册目录监控: %s", config->scan_directory);
    watch_directory_recursive(config->scan_directory);

    if (!watcher_running || inotify_fd < 0 || epoll_fd < 0) {
        reset_watcher_startup_state();
        watcher_running = 0;
        return LV_OK;
    }

    log_info("[增量监控] 已注册 %d 个目录监控点", watch_dir_count);
    if (pthread_create(&watcher_thread, NULL, watcher_thread_func, NULL) != 0) {
        log_error("[增量监控] 创建监控线程失败");
        reset_watcher_startup_state();
        watcher_running = 0;
        return LV_ERROR_UNKNOWN;
    }

    watcher_thread_started = 1;

    log_info("[增量监控] 增量文件监控系统初始化完成");
    return LV_OK;
}

lv_error_t video_scanner_stop_watcher_impl(void)
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

    if (watcher_thread_started) {
        pthread_join(watcher_thread, NULL);
        watcher_thread_started = 0;
    }

    watch_dir_count = 0;
    event_count = 0;
    log_info("[增量监控] 文件监控已停止");
    return LV_OK;
}
