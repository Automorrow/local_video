#ifndef VIDEO_SCANNER_INTERNAL_H
#define VIDEO_SCANNER_INTERNAL_H

#include "video_scanner.h"
#include <pthread.h>
#include <stdint.h>
#include <time.h>

#define MAX_WATCH_DIRS 2048
#define DEBOUNCE_MS 500
#define BATCH_SIZE 64
#define MAX_EPOLL_EVENTS 64

typedef struct {
    int wd;
    char path[1024];
} watch_dir_entry_t;

typedef struct {
    char path[1024];
    uint32_t mask;
    struct timespec timestamp;
} pending_event_t;

extern volatile int watcher_running;
extern int watcher_thread_started;
extern int inotify_fd;
extern int epoll_fd;
extern pthread_t watcher_thread;
extern watch_dir_entry_t watch_dirs[MAX_WATCH_DIRS];
extern int watch_dir_count;
extern pending_event_t event_queue[BATCH_SIZE];
extern int event_count;
extern pthread_mutex_t event_mutex;
extern volatile int scan_in_progress;

int video_scanner_is_video_file(const char *filename);
void video_scanner_extract_title(const char *path, char *title, size_t title_size);
void video_scanner_extract_directory(const char *full_path, char *dir,
                                     size_t dir_size);
const char *video_scanner_find_watch_path(int wd);

lv_error_t video_scanner_start_watcher_impl(void);
lv_error_t video_scanner_stop_watcher_impl(void);
lv_error_t video_scanner_scan_impl(const char *directory);

#endif
