#ifndef LOCAL_VIDEO_H
#define LOCAL_VIDEO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* 错误码定义 */
typedef enum {
    LV_OK = 0,
    LV_ERROR_INVALID_ARG,
    LV_ERROR_MEMORY,
    LV_ERROR_IO,
    LV_ERROR_DB,
    LV_ERROR_NETWORK,
    LV_ERROR_UNKNOWN
} lv_error_t;

/* 日志级别 */
typedef enum {
    LV_LOG_DEBUG,
    LV_LOG_INFO,
    LV_LOG_WARNING,
    LV_LOG_ERROR
} lv_log_level_t;

/* 视频格式 */
typedef enum {
    LV_VIDEO_FORMAT_UNKNOWN,
    LV_VIDEO_FORMAT_MP4,
    LV_VIDEO_FORMAT_WEBM
} lv_video_format_t;

/* 前向声明 */
typedef struct list_node list_node_t;
typedef struct notifier_chain notifier_chain_t;

/* 模块初始化阶段结构体 */
typedef struct {
    const char *name;
    void (*fn)(void);
} module_init_entry_t;

/* 模块订阅阶段结构体 */
typedef struct {
    const char *name;
    void (*fn)(void);
} module_sub_entry_t;

/* 模块运行阶段结构体 */
typedef struct {
    const char *name;
    void (*fn)(void);
} module_run_entry_t;

/* 模块退出阶段结构体 */
typedef struct {
    const char *name;
    void (*fn)(void);
} module_exit_entry_t;

#endif /* LOCAL_VIDEO_H */
