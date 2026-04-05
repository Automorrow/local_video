#include "db_manager.h"
#include "module.h"
#include "../thread/thread.h"
#include "../log/log.h"
#include <stdlib.h>
#include <string.h>

static sqlite3 *g_db = NULL;
static lv_mutex_t g_mutex;
static int g_initialized = 0;

static void db_manager_exit(void) {
    if (g_initialized) {
        lv_mutex_lock(&g_mutex);
        if (g_db) {
            sqlite3_close(g_db);
            g_db = NULL;
        }
        lv_mutex_unlock(&g_mutex);
        lv_mutex_destroy(&g_mutex);
        g_initialized = 0;
    }
}

static void db_manager_init_internal(void) {
    if (g_initialized) {
        return;
    }
    lv_error_t err = lv_mutex_init(&g_mutex);
    if (err != LV_OK) {
        log_error("Failed to initialize mutex for db_manager");
        return;
    }
    g_initialized = 1;
    
    /* Open the database with default path */
    db_manager_init("./local_video.db");
}

MODULE_INIT(db_manager_init_internal, "db_manager");
MODULE_EXIT(db_manager_exit, "db_manager");

lv_error_t db_manager_init(const char *db_path) {
    if (!db_path) {
        return LV_ERROR_INVALID_ARG;
    }
    lv_mutex_lock(&g_mutex);
    if (g_db) {
        lv_mutex_unlock(&g_mutex);
        return LV_OK;
    }
    int rc = sqlite3_open(db_path, &g_db);
    if (rc != SQLITE_OK) {
        log_error("Cannot open database: %s", sqlite3_errmsg(g_db));
        sqlite3_close(g_db);
        g_db = NULL;
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }
    const char *sql =
        "CREATE TABLE IF NOT EXISTS videos ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    path TEXT UNIQUE NOT NULL,"
        "    title TEXT NOT NULL,"
        "    category TEXT NOT NULL,"
        "    size INTEGER NOT NULL,"
        "    blacklisted INTEGER DEFAULT 0,"
        "    created_at INTEGER DEFAULT (strftime('%s', 'now'))"
        ");"
        "CREATE TABLE IF NOT EXISTS history ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    video_id INTEGER NOT NULL,"
        "    position INTEGER DEFAULT 0,"
        "    played_at INTEGER DEFAULT (strftime('%s', 'now')),"
        "    FOREIGN KEY (video_id) REFERENCES videos(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS favorites ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    video_id INTEGER UNIQUE NOT NULL,"
        "    created_at INTEGER DEFAULT (strftime('%s', 'now')),"
        "    FOREIGN KEY (video_id) REFERENCES videos(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS blacklist ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    path TEXT UNIQUE NOT NULL,"
        "    created_at INTEGER DEFAULT (strftime('%s', 'now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_videos_category ON videos(category);"
        "CREATE INDEX IF NOT EXISTS idx_videos_path ON videos(path);"
        "CREATE INDEX IF NOT EXISTS idx_history_video ON history(video_id);";

    char *err_msg = NULL;
    rc = sqlite3_exec(g_db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        log_error("SQL error: %s", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(g_db);
        g_db = NULL;
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    const char *alter_sql = "ALTER TABLE videos ADD COLUMN blacklisted INTEGER DEFAULT 0;";
    err_msg = NULL;
    rc = sqlite3_exec(g_db, alter_sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (strstr(err_msg, "duplicate column name") == NULL) {
            log_error("ALTER TABLE error: %s", err_msg);
            sqlite3_free(err_msg);
            sqlite3_close(g_db);
            g_db = NULL;
            lv_mutex_unlock(&g_mutex);
            return LV_ERROR_DB;
        }
        log_info("Column 'blacklisted' already exists in videos table");
        sqlite3_free(err_msg);
    } else {
        log_info("Added 'blacklisted' column to videos table");
    }
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}

lv_error_t db_manager_close(void) {
    lv_mutex_lock(&g_mutex);
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    lv_mutex_unlock(&g_mutex);
    return LV_OK;
}

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
        video.id = sqlite3_column_int64(stmt, 0);
        const char *path_str = (const char *)sqlite3_column_text(stmt, 1);
        const char *title_str = (const char *)sqlite3_column_text(stmt, 2);
        const char *category_str = (const char *)sqlite3_column_text(stmt, 3);
        if (path_str) strncpy(video.path, path_str, sizeof(video.path) - 1);
        video.path[sizeof(video.path) - 1] = '\0';
        if (title_str) strncpy(video.title, title_str, sizeof(video.title) - 1);
        video.title[sizeof(video.title) - 1] = '\0';
        if (category_str) strncpy(video.category, category_str, sizeof(video.category) - 1);
        video.category[sizeof(video.category) - 1] = '\0';
        video.size = sqlite3_column_int64(stmt, 4);
        video.created_at = sqlite3_column_int64(stmt, 5);
        int ret = callback(&video, user_data);
        if (ret != 0) {
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
        video.id = sqlite3_column_int64(stmt, 0);
        const char *path_str = (const char *)sqlite3_column_text(stmt, 1);
        const char *title_str = (const char *)sqlite3_column_text(stmt, 2);
        const char *category_str = (const char *)sqlite3_column_text(stmt, 3);
        if (path_str) strncpy(video.path, path_str, sizeof(video.path) - 1);
        video.path[sizeof(video.path) - 1] = '\0';
        if (title_str) strncpy(video.title, title_str, sizeof(video.title) - 1);
        video.title[sizeof(video.title) - 1] = '\0';
        if (category_str) strncpy(video.category, category_str, sizeof(video.category) - 1);
        video.category[sizeof(video.category) - 1] = '\0';
        video.size = sqlite3_column_int64(stmt, 4);
        video.created_at = sqlite3_column_int64(stmt, 5);
        int ret = callback(&video, user_data);
        if (ret != 0) {
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
        int blacklisted = sqlite3_column_int(stmt, 6);
        if (blacklisted) {
            err = LV_ERROR_DB;
        } else {
            out->id = sqlite3_column_int64(stmt, 0);
            const char *path_str = (const char *)sqlite3_column_text(stmt, 1);
            const char *title_str = (const char *)sqlite3_column_text(stmt, 2);
            const char *category_str = (const char *)sqlite3_column_text(stmt, 3);
            if (path_str) strncpy(out->path, path_str, sizeof(out->path) - 1);
            out->path[sizeof(out->path) - 1] = '\0';
            if (title_str) strncpy(out->title, title_str, sizeof(out->title) - 1);
            out->title[sizeof(out->title) - 1] = '\0';
            if (category_str) strncpy(out->category, category_str, sizeof(out->category) - 1);
            out->category[sizeof(out->category) - 1] = '\0';
            out->size = sqlite3_column_int64(stmt, 4);
            out->created_at = sqlite3_column_int64(stmt, 5);
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
        video.id = sqlite3_column_int64(stmt, 0);
        const char *path_str = (const char *)sqlite3_column_text(stmt, 1);
        const char *title_str = (const char *)sqlite3_column_text(stmt, 2);
        const char *category_str = (const char *)sqlite3_column_text(stmt, 3);
        if (path_str) strncpy(video.path, path_str, sizeof(video.path) - 1);
        video.path[sizeof(video.path) - 1] = '\0';
        if (title_str) strncpy(video.title, title_str, sizeof(video.title) - 1);
        video.title[sizeof(video.title) - 1] = '\0';
        if (category_str) strncpy(video.category, category_str, sizeof(video.category) - 1);
        video.category[sizeof(video.category) - 1] = '\0';
        video.size = sqlite3_column_int64(stmt, 4);
        video.created_at = sqlite3_column_int64(stmt, 5);
        int ret = callback(&video, user_data);
        if (ret != 0) {
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
        video.id = sqlite3_column_int64(stmt, 0);
        const char *path_str = (const char *)sqlite3_column_text(stmt, 1);
        const char *title_str = (const char *)sqlite3_column_text(stmt, 2);
        const char *category_str = (const char *)sqlite3_column_text(stmt, 3);
        if (path_str) strncpy(video.path, path_str, sizeof(video.path) - 1);
        video.path[sizeof(video.path) - 1] = '\0';
        if (title_str) strncpy(video.title, title_str, sizeof(video.title) - 1);
        video.title[sizeof(video.title) - 1] = '\0';
        if (category_str) strncpy(video.category, category_str, sizeof(video.category) - 1);
        video.category[sizeof(video.category) - 1] = '\0';
        video.size = sqlite3_column_int64(stmt, 4);
        video.created_at = sqlite3_column_int64(stmt, 5);
        int ret = callback(&video, user_data);
        if (ret != 0) {
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

lv_error_t db_manager_history_add(int64_t video_id, int64_t position) {
    lv_mutex_lock(&g_mutex);
    const char *sql = "INSERT INTO history (video_id, position) VALUES (?, ?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
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
        history.id = sqlite3_column_int64(stmt, 0);
        history.video_id = sqlite3_column_int64(stmt, 1);
        const char *title_str = (const char *)sqlite3_column_text(stmt, 2);
        const char *path_str = (const char *)sqlite3_column_text(stmt, 3);
        if (title_str) strncpy(history.video_title, title_str, sizeof(history.video_title) - 1);
        history.video_title[sizeof(history.video_title) - 1] = '\0';
        if (path_str) strncpy(history.video_path, path_str, sizeof(history.video_path) - 1);
        history.video_path[sizeof(history.video_path) - 1] = '\0';
        history.position = sqlite3_column_int64(stmt, 4);
        history.played_at = sqlite3_column_int64(stmt, 5);
        int ret = callback(&history, user_data);
        if (ret != 0) {
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
        favorite.id = sqlite3_column_int64(stmt, 0);
        favorite.video_id = sqlite3_column_int64(stmt, 1);
        const char *title_str = (const char *)sqlite3_column_text(stmt, 2);
        const char *path_str = (const char *)sqlite3_column_text(stmt, 3);
        if (title_str) strncpy(favorite.video_title, title_str, sizeof(favorite.video_title) - 1);
        favorite.video_title[sizeof(favorite.video_title) - 1] = '\0';
        if (path_str) strncpy(favorite.video_path, path_str, sizeof(favorite.video_path) - 1);
        favorite.video_path[sizeof(favorite.video_path) - 1] = '\0';
        favorite.created_at = sqlite3_column_int64(stmt, 4);
        int ret = callback(&favorite, user_data);
        if (ret != 0) {
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
        video.id = sqlite3_column_int64(stmt, 0);
        const char *path_str = (const char *)sqlite3_column_text(stmt, 1);
        const char *title_str = (const char *)sqlite3_column_text(stmt, 2);
        const char *category_str = (const char *)sqlite3_column_text(stmt, 3);
        if (path_str) strncpy(video.path, path_str, sizeof(video.path) - 1);
        video.path[sizeof(video.path) - 1] = '\0';
        if (title_str) strncpy(video.title, title_str, sizeof(video.title) - 1);
        video.title[sizeof(video.title) - 1] = '\0';
        if (category_str) strncpy(video.category, category_str, sizeof(video.category) - 1);
        video.category[sizeof(video.category) - 1] = '\0';
        video.size = sqlite3_column_int64(stmt, 4);
        video.created_at = sqlite3_column_int64(stmt, 5);
        int ret = callback(&video, user_data);
        if (ret != 0) {
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
    sqlite3_stmt *stmt;
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
        entry.id = sqlite3_column_int64(stmt, 0);
        const char *path_str = (const char *)sqlite3_column_text(stmt, 1);
        if (path_str) strncpy(entry.path, path_str, sizeof(entry.path) - 1);
        entry.path[sizeof(entry.path) - 1] = '\0';
        entry.created_at = sqlite3_column_int64(stmt, 2);
        int ret = callback(&entry, user_data);
        if (ret != 0) {
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
        const char *path_str = (const char *)sqlite3_column_text(stmt, 0);
        if (path_str) {
            strncpy(out_path, path_str, path_size - 1);
            out_path[path_size - 1] = '\0';
        }
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
