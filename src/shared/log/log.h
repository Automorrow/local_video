#ifndef LOG_H
#define LOG_H

#include "local_video.h"
#include <stdio.h>
#include <stdarg.h>

void log_set_level(lv_log_level_t level);
void log_set_file(FILE *file);

void log_debug(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_warning(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif /* LOG_H */
