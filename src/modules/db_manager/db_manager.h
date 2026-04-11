#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include "../../include/local_video.h"
#include <sqlite3.h>

typedef struct {
    int64_t id;
    char path[512];
    char title[256];
    char category[256];
    int64_t size;
    int64_t created_at;
} VideoInfo;

typedef struct {
    int64_t id;
    int64_t video_id;
    char video_title[256];
    char video_path[512];
    int64_t position;
    int64_t played_at;
} HistoryInfo;

typedef struct {
    int64_t id;
    int64_t video_id;
    char video_title[256];
    char video_path[512];
    int64_t created_at;
} FavoriteInfo;

typedef struct {
    int64_t id;
    char path[512];
    int64_t created_at;
} BlacklistInfo;

typedef int (*video_callback_t)(const VideoInfo *video, void *user_data);
typedef int (*history_callback_t)(const HistoryInfo *history, void *user_data);
typedef int (*favorite_callback_t)(const FavoriteInfo *favorite, void *user_data);
typedef int (*category_callback_t)(const char *category, void *user_data);
typedef int (*blacklist_callback_t)(const BlacklistInfo *entry, void *user_data);

lv_error_t db_manager_init(const char *db_path);
lv_error_t db_manager_close(void);

lv_error_t db_manager_video_insert(const char *path, const char *title, const char *category, int64_t size);
lv_error_t db_manager_video_get_all(video_callback_t callback, void *user_data);
lv_error_t db_manager_video_get_all_paginated(video_callback_t callback, void *user_data, int limit, int offset);
lv_error_t db_manager_video_get_by_id(int64_t id, VideoInfo *out);
lv_error_t db_manager_video_search(const char *query, video_callback_t callback, void *user_data);
lv_error_t db_manager_video_get_by_category(const char *category, video_callback_t callback, void *user_data);
lv_error_t db_manager_video_delete(int64_t id);

lv_error_t db_manager_history_add(int64_t video_id, int64_t position);
lv_error_t db_manager_history_get(history_callback_t callback, void *user_data);
lv_error_t db_manager_history_delete(int64_t id);
lv_error_t db_manager_history_clear(void);

lv_error_t db_manager_favorite_add(int64_t video_id);
lv_error_t db_manager_favorite_remove(int64_t video_id);
lv_error_t db_manager_favorites_list(favorite_callback_t callback, void *user_data);
lv_error_t db_manager_favorite_check(int64_t video_id, bool *out);

lv_error_t db_manager_video_get_random(int count, video_callback_t callback, void *user_data);
lv_error_t db_manager_video_count(int64_t *out_count);
lv_error_t db_manager_category_get_all(category_callback_t callback, void *user_data);

lv_error_t db_manager_blacklist_add(const char *path, int64_t *out_id);
lv_error_t db_manager_blacklist_delete(int64_t id);
lv_error_t db_manager_blacklist_get_all(blacklist_callback_t callback, void *user_data);
lv_error_t db_manager_blacklist_check(const char *path, bool *out);
lv_error_t db_manager_blacklist_get_by_id(int64_t id, char *out_path, size_t path_size);
lv_error_t db_manager_video_delete_by_path_prefix(const char *prefix);
lv_error_t db_manager_video_blacklist_by_path_prefix(const char *prefix);
lv_error_t db_manager_video_unblacklist_all(void);
lv_error_t db_manager_video_unblacklist_by_path_prefix(const char *prefix);
lv_error_t db_manager_video_update(const char *path, const char *title, const char *category, int64_t size);
lv_error_t db_manager_video_delete_by_path(const char *path);
lv_error_t db_manager_video_search_by_path_substr(const char *substr, char *out_path, size_t out_size);

#endif /* DB_MANAGER_H */
