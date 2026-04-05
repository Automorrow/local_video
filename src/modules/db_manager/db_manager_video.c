#include "db_manager_internal.h"
#include "../../shared/log/log.h"
#include <stdio.h>

lv_error_t db_manager_video_insert(const char *path, const char *title, const char *category, int64_t size) {
    if (!path || !title || !category) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_error_t err = LV_OK;
    lv_mutex_lock(&g_mutex);
    const char *sql = "INSERT OR IGNORE INTO videos (path, title, category, size) VALUES (?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, category, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, size);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        err = LV_ERROR_DB;
    }
    lv_mutex_unlock(&g_mutex);
    return err;
}

lv_error_t db_manager_video_get_all(video_callback_t callback, void *user_data) {
    if (!callback) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_error_t err = LV_OK;
    lv_mutex_lock(&g_mutex);
    const char *sql = "SELECT id, path, title, category, size, created_at FROM videos WHERE blacklisted = 0 ORDER BY id DESC";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        VideoInfo video;
        db_fill_video_info(stmt, &video);
        if (callback(&video, user_data) != 0) {
            break;
        }
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        err = LV_ERROR_DB;
    }
    lv_mutex_unlock(&g_mutex);
    return err;
}

lv_error_t db_manager_video_get_all_paginated(video_callback_t callback, void *user_data, int limit, int offset)
{
    if (!callback || limit < 0 || offset < 0) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_error_t err = LV_OK;
    lv_mutex_lock(&g_mutex);

    const char *sql = "SELECT id, path, title, category, size, created_at FROM videos WHERE blacklisted = 0 ORDER BY id DESC LIMIT ? OFFSET ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    sqlite3_bind_int(stmt, 1, limit);
    sqlite3_bind_int(stmt, 2, offset);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        VideoInfo video;
        db_fill_video_info(stmt, &video);
        if (callback(&video, user_data) != 0) {
            break;
        }
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        err = LV_ERROR_DB;
    }
    lv_mutex_unlock(&g_mutex);
    return err;
}

lv_error_t db_manager_video_get_by_id(int64_t id, VideoInfo *out) {
    if (!out) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_error_t err = LV_OK;
    lv_mutex_lock(&g_mutex);
    const char *sql = "SELECT id, path, title, category, size, created_at, blacklisted FROM videos WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_int64(stmt, 1, id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        if (sqlite3_column_int(stmt, 6) != 0) {
            err = LV_ERROR_DB;
        } else {
            db_fill_video_info(stmt, out);
        }
    } else {
        err = LV_ERROR_DB;
    }
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    return err;
}

lv_error_t db_manager_video_search(const char *query, video_callback_t callback, void *user_data) {
    if (!query || !callback) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_error_t err = LV_OK;
    lv_mutex_lock(&g_mutex);
    const char *sql = "SELECT id, path, title, category, size, created_at FROM videos "
                      "WHERE blacklisted = 0 AND (title LIKE ? OR category LIKE ? OR path LIKE ?) "
                      "ORDER BY "
                      "CASE WHEN title LIKE ? THEN 1 "
                      "WHEN path LIKE ? THEN 2 "
                      "WHEN category LIKE ? THEN 3 "
                      "ELSE 4 END, "
                      "id DESC";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    char search_pattern[512];
    snprintf(search_pattern, sizeof(search_pattern), "%%%s%%", query);
    sqlite3_bind_text(stmt, 1, search_pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, search_pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, search_pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, search_pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, search_pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, search_pattern, -1, SQLITE_TRANSIENT);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        VideoInfo video;
        db_fill_video_info(stmt, &video);
        if (callback(&video, user_data) != 0) {
            break;
        }
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        err = LV_ERROR_DB;
    }
    lv_mutex_unlock(&g_mutex);
    return err;
}

lv_error_t db_manager_video_get_by_category(const char *category, video_callback_t callback, void *user_data) {
    if (!category || !callback) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_error_t err = LV_OK;
    lv_mutex_lock(&g_mutex);
    const char *sql = "SELECT id, path, title, category, size, created_at FROM videos WHERE category = ? ORDER BY id DESC";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_text(stmt, 1, category, -1, SQLITE_TRANSIENT);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        VideoInfo video;
        db_fill_video_info(stmt, &video);
        if (callback(&video, user_data) != 0) {
            break;
        }
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        err = LV_ERROR_DB;
    }
    lv_mutex_unlock(&g_mutex);
    return err;
}

lv_error_t db_manager_video_delete(int64_t id) {
    lv_mutex_lock(&g_mutex);
    const char *sql = "DELETE FROM videos WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_int64(stmt, 1, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    if (rc != SQLITE_DONE) {
        return LV_ERROR_DB;
    }
    return LV_OK;
}

lv_error_t db_manager_video_update(const char *path, const char *title, const char *category, int64_t size) {
    if (!path || !title || !category) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql = "UPDATE videos SET title = ?, category = ?, size = ? WHERE path = ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, category, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, size);
    sqlite3_bind_text(stmt, 4, path, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    return (rc == SQLITE_DONE) ? LV_OK : LV_ERROR_DB;
}

lv_error_t db_manager_video_delete_by_path(const char *path) {
    if (!path) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql = "DELETE FROM videos WHERE path = ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    log_debug("Deleted video by path: %s (changes=%d)", path, changes);
    lv_mutex_unlock(&g_mutex);
    return (rc == SQLITE_DONE) ? LV_OK : LV_ERROR_DB;
}

lv_error_t db_manager_video_get_random(int count, video_callback_t callback, void *user_data) {
    if (!callback || count <= 0) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_error_t err = LV_OK;
    lv_mutex_lock(&g_mutex);
    const char *sql = "SELECT id, path, title, category, size, created_at FROM videos WHERE blacklisted = 0 ORDER BY RANDOM() LIMIT ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_int(stmt, 1, count);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        VideoInfo video;
        db_fill_video_info(stmt, &video);
        if (callback(&video, user_data) != 0) {
            break;
        }
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        err = LV_ERROR_DB;
    }
    lv_mutex_unlock(&g_mutex);
    return err;
}

lv_error_t db_manager_video_count(int64_t *out_count) {
    if (!out_count) {
        return LV_ERROR_INVALID_ARG;
    }
    *out_count = 0;
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql = "SELECT COUNT(*) FROM videos WHERE blacklisted = 0";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *out_count = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}

lv_error_t db_manager_category_get_all(category_callback_t callback, void *user_data)
{
    if (!callback) {
        return LV_ERROR_INVALID_ARG;
    }

    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    const char *sql = "SELECT DISTINCT category FROM videos ORDER BY category";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    lv_error_t err = LV_OK;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *category = (const char *)sqlite3_column_text(stmt, 0);
        if (category) {
            int ret = callback(category, user_data);
            if (ret != 0) {
                break;
            }
        }
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        err = LV_ERROR_DB;
    }
    lv_mutex_unlock(&g_mutex);
    return err;
}
