#ifndef VIDEO_SCANNER_H
#define VIDEO_SCANNER_H

#include "local_video.h"

lv_error_t video_scanner_scan(const char *directory);
lv_error_t video_scanner_rescan(void);
lv_error_t video_scanner_start_watcher(void);
lv_error_t video_scanner_stop_watcher(void);

#endif /* VIDEO_SCANNER_H */
