#include "log.h"
#include <time.h>

static lv_log_level_t current_level = LV_LOG_INFO;
static FILE *current_file = NULL;

void log_set_level(lv_log_level_t level)
{
    current_level = level;
}

void log_set_file(FILE *file)
{
    current_file = file;
}

static void log_vprintf(lv_log_level_t level, const char *level_str, const char *fmt, va_list args)
{
    if (level < current_level) {
        return;
    }

    FILE *out = current_file ? current_file : stderr;
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    fprintf(out, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            level_str);

    vfprintf(out, fmt, args);
    fprintf(out, "\n");
    fflush(out);
}

void log_debug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_vprintf(LV_LOG_DEBUG, "DEBUG", fmt, args);
    va_end(args);
}

void log_info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_vprintf(LV_LOG_INFO, "INFO", fmt, args);
    va_end(args);
}

void log_warning(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_vprintf(LV_LOG_WARNING, "WARNING", fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_vprintf(LV_LOG_ERROR, "ERROR", fmt, args);
    va_end(args);
}
