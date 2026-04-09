#include "video_scanner_internal.h"
#include "../db_manager/db_manager.h"
#include "../config/config.h"
#include "../../shared/log/log.h"
#include "../../include/platform.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
/* ====== Windows Implementation (ReadDirectoryChangesW) ====== */
#include <windows.h>

#define MAX_WATCH_DIRS_WIN 2048
#define WATCH_BUFFER_SIZE (64 * 1024)

typedef struct {
    HANDLE dir_handle;
    OVERLAPPED overlapped;
    char path[4096];
    uint8_t buffer[WATCH_BUFFER_SIZE];
    volatile int active;
} win_watch_entry_t;

static win_watch_entry_t *win_watches[MAX_WATCH_DIRS_WIN];
static int win_watch_count = 0;
static HANDLE watcher_thread_handle = NULL;
static volatile int win_watcher_running = 0;

/* Forward declarations */
static void win_process_video_create(const char *real_path);
static void win_process_video_delete(const char *real_path);
static void win_process_video_modify(const char *real_path);
static void win_handle_notification(win_watch_entry_t *watch,
                                    FILE_NOTIFY_INFORMATION *info);
static DWORD WINAPI win_watch_directory_thread(LPVOID arg);
static int win_watch_directory_recursive(const char *path);

static void win_process_video_create(const char *real_path)
{
    struct _stat64 st;
    if (_stat64(real_path, &st) < 0 || st.st_size < 1024 * 1024) return;

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
        log_info("[增量] 新视频: %s (%lld bytes)", title, (long long)st.st_size);
    }
}

static void win_process_video_delete(const char *real_path)
{
    lv_error_t err = db_manager_video_delete_by_path(real_path);
    if (err == LV_OK) {
        const char *filename = strrchr(real_path, '/');
        log_info("[增量] 视频已删除: %s", filename ? filename + 1 : real_path);
    }
}

static void win_process_video_modify(const char *real_path)
{
    struct _stat64 st;
    if (_stat64(real_path, &st) < 0) return;

    char title[256];
    char category[512];
    video_scanner_extract_title(real_path, title, sizeof(title));
    video_scanner_extract_directory(real_path, category, sizeof(category));

    lv_error_t err = db_manager_video_update(real_path, title, category, st.st_size);
    if (err == LV_OK) {
        log_debug("[增量] 视频已更新: %s", title);
    }
}

static void win_handle_notification(win_watch_entry_t *watch,
                                    FILE_NOTIFY_INFORMATION *info)
{
    /* Convert wide filename to UTF-8 */
    char utf8_name[1024];
    int name_len = WideCharToMultiByte(CP_UTF8, 0, info->FileName,
                                       info->FileNameLength / sizeof(WCHAR),
                                       utf8_name, sizeof(utf8_name) - 1, NULL, NULL);
    if (name_len <= 0) return;
    utf8_name[name_len] = '\0';

    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s/%s", watch->path, utf8_name);

    /* Resolve real path */
    char real_path[4096];
    if (_fullpath(real_path, full_path, sizeof(real_path)) == NULL) {
        log_debug("[增量] 无法解析路径: %s", full_path);
        return;
    }

    /* Normalize to forward slash */
    for (char *p = real_path; *p; p++) {
        if (*p == '\\') *p = '/';
    }

    if (!video_scanner_is_video_file(real_path)) return;

    switch (info->Action) {
    case FILE_ACTION_ADDED:
    case FILE_ACTION_RENAMED_NEW_NAME:
        win_process_video_create(real_path);
        /* If it's a new directory, watch it recursively */
        {
            struct _stat64 st;
            if (_stat64(real_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                log_info("[增量] 新子目录: %s", real_path);
                win_watch_directory_recursive(real_path);
            }
        }
        break;
    case FILE_ACTION_REMOVED:
    case FILE_ACTION_RENAMED_OLD_NAME:
        win_process_video_delete(real_path);
        break;
    case FILE_ACTION_MODIFIED:
        win_process_video_modify(real_path);
        break;
    }
}

static DWORD WINAPI win_watch_directory_thread(LPVOID arg)
{
    win_watch_entry_t *watch = (win_watch_entry_t *)arg;

    while (watch->active && win_watcher_running) {
        DWORD bytes_returned;
        BOOL success = ReadDirectoryChangesW(
            watch->dir_handle,
            watch->buffer,
            sizeof(watch->buffer),
            TRUE,  /* watch subtree */
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_CREATION |
            FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytes_returned,
            &watch->overlapped,
            NULL
        );

        if (!success) {
            DWORD err = GetLastError();
            if (err == ERROR_INVALID_HANDLE || !watch->active || !win_watcher_running) {
                break;
            }
            /* For other errors, wait a bit and retry */
            Sleep(1000);
            continue;
        }

        /* Process notifications */
        if (bytes_returned > 0) {
            BYTE *ptr = watch->buffer;
            while (ptr < watch->buffer + bytes_returned) {
                FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION *)ptr;
                win_handle_notification(watch, info);
                if (info->NextEntryOffset == 0) break;
                ptr += info->NextEntryOffset;
            }
        }
    }

    return 0;
}

static int win_watch_directory_recursive(const char *path)
{
    if (!win_watcher_running) return -1;

    bool in_blacklist = false;
    if (db_manager_blacklist_check(path, &in_blacklist) == LV_OK && in_blacklist) {
        log_debug("Skipping blacklisted directory watch: %s", path);
        return -1;
    }

    if (win_watch_count >= MAX_WATCH_DIRS_WIN) {
        log_warning("[增量监控] 已达到最大监控目录数: %d", MAX_WATCH_DIRS_WIN);
        return -1;
    }

    /* Open directory with FILE_LIST_DIRECTORY */
    WCHAR wpath[4096];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 4096);

    HANDLE hDir = CreateFileW(
        wpath,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        log_warning("[增量监控] 无法打开目录: %s (error=%lu)", path, GetLastError());
        return -1;
    }

    win_watch_entry_t *entry = (win_watch_entry_t *)calloc(1, sizeof(win_watch_entry_t));
    if (!entry) {
        CloseHandle(hDir);
        return -1;
    }

    entry->dir_handle = hDir;
    memset(&entry->overlapped, 0, sizeof(entry->overlapped));
    strncpy(entry->path, path, sizeof(entry->path) - 1);
    entry->active = 1;

    win_watches[win_watch_count] = entry;
    win_watch_count++;

    log_debug("[增量监控] 监控目录: %s (total=%d)", path, win_watch_count);

    /* Create a thread for this directory */
    HANDLE hThread = CreateThread(NULL, 0, win_watch_directory_thread, entry, 0, NULL);
    if (hThread == NULL) {
        log_warning("[增量监控] 无法创建监控线程: %s", path);
        entry->active = 0;
        CloseHandle(hDir);
        free(entry);
        win_watch_count--;
        return -1;
    }
    CloseHandle(hThread);  /* Thread runs independently */

    /* Recursively watch subdirectories */
    char search_path[4096];
    snprintf(search_path, sizeof(search_path), "%s\\*", path);
    WCHAR wsearch[4096];
    MultiByteToWideChar(CP_UTF8, 0, search_path, -1, wsearch, 4096);

    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(wsearch, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
                continue;
            }
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (find_data.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_REPARSE_POINT)) {
                continue;
            }

            char utf8_name[1024];
            WideCharToMultiByte(CP_UTF8, 0, find_data.cFileName, -1,
                               utf8_name, sizeof(utf8_name), NULL, NULL);

            char sub_path[4096];
            snprintf(sub_path, sizeof(sub_path), "%s/%s", path, utf8_name);
            win_watch_directory_recursive(sub_path);
        } while (FindNextFileW(hFind, &find_data));
        FindClose(hFind);
    }

    return 0;
}

static void win_stop_all_watches(void)
{
    for (int i = 0; i < win_watch_count; i++) {
        if (win_watches[i]) {
            win_watches[i]->active = 0;
            CancelIoEx(&win_watches[i]->overlapped, NULL);
            /* Give thread time to exit */
            Sleep(100);
            CloseHandle(win_watches[i]->dir_handle);
            free(win_watches[i]);
            win_watches[i] = NULL;
        }
    }
    win_watch_count = 0;
}

lv_error_t video_scanner_start_watcher_impl(void)
{
    if (win_watcher_running) {
        log_warning("[增量监控] 监控已在运行");
        return LV_OK;
    }

    const lv_config_t *config = config_get();
    if (!config || !config->scan_directory) {
        log_error("[增量监控] 未配置扫描目录");
        return LV_ERROR_INVALID_ARG;
    }

    win_watcher_running = 1;
    win_watch_count = 0;

    log_info("[增量监控] 注册目录监控: %s", config->scan_directory);
    win_watch_directory_recursive(config->scan_directory);

    if (win_watch_count == 0) {
        log_warning("[增量监控] 未成功注册任何目录监控");
        win_watcher_running = 0;
        return LV_ERROR_UNKNOWN;
    }

    log_info("[增量监控] 已注册 %d 个目录监控点", win_watch_count);
    log_info("[增量监控] 增量文件监控系统初始化完成");
    return LV_OK;
}

lv_error_t video_scanner_stop_watcher_impl(void)
{
    if (!win_watcher_running) return LV_OK;

    log_info("[增量监控] 正在停止文件监控...");
    win_watcher_running = 0;

    win_stop_all_watches();

    log_info("[增量监控] 文件监控已停止");
    return LV_OK;
}

#else /* POSIX / Linux */
/* ====== Linux Implementation (inotify + epoll) ====== */
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
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

#endif /* _WIN32 */
