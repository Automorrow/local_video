#include "db_manager_internal.h"
#include "../../shared/log/log.h"
#include <string.h>

lv_error_t db_manager_video_blacklist_by_path_prefix(const char *prefix) {
    if (!prefix) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql = "UPDATE videos SET blacklisted = 1 WHERE path LIKE ? || '%'";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_text(stmt, 1, prefix, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_error("Step error: %s", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    log_info("Blacklisted %d videos with prefix: %s", changes, prefix);
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}

lv_error_t db_manager_video_unblacklist_all(void) {
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql = "UPDATE videos SET blacklisted = 0";
    char *err_msg = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        log_error("SQL error: %s", err_msg);
        sqlite3_free(err_msg);
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    log_info("Unblacklisted all videos");
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}

lv_error_t db_manager_blacklist_add(const char *path, int64_t *out_id) {
    if (!path) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql = "INSERT INTO blacklist (path) VALUES (?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_error("Step error: %s", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    if (out_id) {
        *out_id = sqlite3_last_insert_rowid(g_db);
    }
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}

lv_error_t db_manager_blacklist_delete(int64_t id) {
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql = "DELETE FROM blacklist WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_int64(stmt, 1, id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_error("Step error: %s", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}

lv_error_t db_manager_blacklist_get_all(blacklist_callback_t callback, void *user_data) {
    if (!callback) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_error_t err = LV_OK;
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql = "SELECT id, path, created_at FROM blacklist ORDER BY id";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        BlacklistInfo entry;
        db_fill_blacklist_info(stmt, &entry);
        if (callback(&entry, user_data) != 0) {
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

lv_error_t db_manager_blacklist_check(const char *path, bool *out) {
    if (!path || !out) {
        return LV_ERROR_INVALID_ARG;
    }
    *out = false;
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql = "SELECT path FROM blacklist";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *blacklist_path = (const char *)sqlite3_column_text(stmt, 0);
        if (blacklist_path) {
            size_t blacklist_len = strlen(blacklist_path);
            if (strncmp(path, blacklist_path, blacklist_len) == 0) {
                *out = true;
                break;
            }
        }
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}

lv_error_t db_manager_blacklist_get_by_id(int64_t id, char *out_path, size_t path_size) {
    if (!out_path || path_size == 0) {
        return LV_ERROR_INVALID_ARG;
    }
    out_path[0] = '\0';
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql = "SELECT path FROM blacklist WHERE id = ?";
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
        db_copy_text(out_path, path_size, sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}

lv_error_t db_manager_video_unblacklist_by_path_prefix(const char *prefix) {
    if (!prefix) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql = "UPDATE videos SET blacklisted = 0 WHERE path LIKE ? || '%'";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_text(stmt, 1, prefix, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_error("Step error: %s", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    log_info("Unblacklisted %d videos with prefix: %s", changes, prefix);
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}

lv_error_t db_manager_video_delete_by_path_prefix(const char *prefix) {
    if (!prefix) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql = "DELETE FROM videos WHERE path LIKE ? || '%'";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Prepare error: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_bind_text(stmt, 1, prefix, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_error("Step error: %s", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}
