#include "db_manager_internal.h"
#include "../../shared/log/log.h"

lv_error_t db_manager_history_add(int64_t video_id, int64_t position) {
    lv_mutex_lock(&g_mutex);

    /* Check the most recent history record */
    int64_t last_video_id = -1;
    int64_t last_id = -1;
    const char *check_sql = "SELECT id, video_id FROM history ORDER BY played_at DESC LIMIT 1";
    sqlite3_stmt *check_stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, check_sql, -1, &check_stmt, NULL);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            last_id = sqlite3_column_int64(check_stmt, 0);
            last_video_id = sqlite3_column_int64(check_stmt, 1);
        }
        sqlite3_finalize(check_stmt);
    }

    /* If the most recent record is the same video, update it instead of inserting */
    if (last_video_id == video_id && last_id != -1) {
        const char *update_sql = "UPDATE history SET position = ?, played_at = strftime('%s', 'now') WHERE id = ?";
        sqlite3_stmt *update_stmt = NULL;
        rc = sqlite3_prepare_v2(g_db, update_sql, -1, &update_stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int64(update_stmt, 1, position);
            sqlite3_bind_int64(update_stmt, 2, last_id);
            sqlite3_step(update_stmt);
            sqlite3_finalize(update_stmt);
        }
        lv_mutex_unlock(&g_mutex);
        return LV_OK;
    }

    const char *sql = "INSERT INTO history (video_id, position) VALUES (?, ?)";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_int64(stmt, 1, video_id);
    sqlite3_bind_int64(stmt, 2, position);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    if (rc != SQLITE_DONE) {
        return LV_ERROR_DB;
    }
    return LV_OK;
}

lv_error_t db_manager_history_get(history_callback_t callback, void *user_data) {
    if (!callback) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_error_t err = LV_OK;
    lv_mutex_lock(&g_mutex);
    const char *sql = "SELECT h.id, h.video_id, v.title, v.path, h.position, h.played_at FROM history h INNER JOIN videos v ON h.video_id = v.id WHERE v.blacklisted = 0 ORDER BY h.played_at DESC";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        HistoryInfo history;
        db_fill_history_info(stmt, &history);
        if (callback(&history, user_data) != 0) {
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

lv_error_t db_manager_history_delete(int64_t id) {
    lv_mutex_lock(&g_mutex);
    const char *sql = "DELETE FROM history WHERE id = ?";
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

lv_error_t db_manager_history_clear(void) {
    lv_mutex_lock(&g_mutex);
    const char *sql = "DELETE FROM history";
    char *err_msg = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &err_msg);
    lv_mutex_unlock(&g_mutex);
    if (rc != SQLITE_OK) {
        log_error("SQL error: %s", err_msg);
        sqlite3_free(err_msg);
        return LV_ERROR_DB;
    }
    return LV_OK;
}

lv_error_t db_manager_favorite_add(int64_t video_id) {
    lv_mutex_lock(&g_mutex);
    const char *sql = "INSERT OR IGNORE INTO favorites (video_id) VALUES (?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_int64(stmt, 1, video_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    if (rc != SQLITE_DONE) {
        return LV_ERROR_DB;
    }
    return LV_OK;
}

lv_error_t db_manager_favorite_remove(int64_t video_id) {
    lv_mutex_lock(&g_mutex);
    const char *sql = "DELETE FROM favorites WHERE video_id = ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_int64(stmt, 1, video_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    if (rc != SQLITE_DONE) {
        return LV_ERROR_DB;
    }
    return LV_OK;
}

lv_error_t db_manager_favorites_list(favorite_callback_t callback, void *user_data) {
    if (!callback) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_error_t err = LV_OK;
    lv_mutex_lock(&g_mutex);
    const char *sql = "SELECT f.id, f.video_id, v.title, v.path, f.created_at FROM favorites f LEFT JOIN videos v ON f.video_id = v.id ORDER BY f.created_at DESC";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        FavoriteInfo favorite;
        db_fill_favorite_info(stmt, &favorite);
        if (callback(&favorite, user_data) != 0) {
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

lv_error_t db_manager_favorite_check(int64_t video_id, bool *out) {
    if (!out) {
        return LV_ERROR_INVALID_ARG;
    }
    *out = false;
    lv_mutex_lock(&g_mutex);
    const char *sql = "SELECT 1 FROM favorites WHERE video_id = ? LIMIT 1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_int64(stmt, 1, video_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *out = true;
    }
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}
