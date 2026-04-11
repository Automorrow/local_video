#include "db_manager_internal.h"
#include "../../shared/log/log.h"
#include <string.h>

lv_error_t db_manager_setting_get(const char *key, char *value, size_t value_size)
{
    if (!key || !value || value_size == 0) {
        return LV_ERROR_INVALID_ARG;
    }

    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    const char *sql = "SELECT value FROM settings WHERE key = ?;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Failed to prepare setting get: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        db_copy_text(value, value_size, sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        lv_mutex_unlock(&g_mutex);
        return LV_OK;
    }

    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    value[0] = '\0';
    return LV_ERROR_DB;
}

lv_error_t db_manager_setting_set(const char *key, const char *value)
{
    if (!key || !value) {
        return LV_ERROR_INVALID_ARG;
    }

    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    const char *sql = "INSERT INTO settings (key, value) VALUES (?, ?)"
                      " ON CONFLICT(key) DO UPDATE SET value = excluded.value;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Failed to prepare setting set: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);

    return (rc == SQLITE_DONE) ? LV_OK : LV_ERROR_DB;
}
