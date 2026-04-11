#include "db_manager_internal.h"
#include "../../shared/log/log.h"
#include <string.h>

lv_error_t db_manager_directory_upsert(const char *path, const char *name, const char *parent_path)
{
    if (!path || !name) {
        return LV_ERROR_INVALID_ARG;
    }

    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    const char *sql = "INSERT INTO directories (path, name, parent_path, scanned_at)"
                      " VALUES (?, ?, ?, strftime('%s', 'now'))"
                      " ON CONFLICT(path) DO UPDATE SET"
                      " name = excluded.name,"
                      " parent_path = excluded.parent_path,"
                      " scanned_at = excluded.scanned_at;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Failed to prepare directory upsert: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, parent_path ? parent_path : "", -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);

    return (rc == SQLITE_DONE) ? LV_OK : LV_ERROR_DB;
}

lv_error_t db_manager_directory_get_children(const char *parent_path,
    int (*callback)(const char *name, const char *path, void *user_data),
    void *user_data)
{
    if (!parent_path || !callback) {
        return LV_ERROR_INVALID_ARG;
    }

    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    const char *sql = "SELECT name, path FROM directories WHERE parent_path = ? ORDER BY name;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_error("Failed to prepare directory get children: %s", sqlite3_errmsg(g_db));
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    sqlite3_bind_text(stmt, 1, parent_path, -1, SQLITE_STATIC);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(stmt, 0);
        const unsigned char *path = sqlite3_column_text(stmt, 1);
        if (callback((const char *)name, (const char *)path, user_data) != 0) {
            break;
        }
    }
    sqlite3_finalize(stmt);
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}

lv_error_t db_manager_directory_clear(void)
{
    lv_mutex_lock(&g_mutex);
    if (!g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    char *err_msg = NULL;
    int rc = sqlite3_exec(g_db, "DELETE FROM directories;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        log_error("Failed to clear directories: %s", err_msg);
        sqlite3_free(err_msg);
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}
