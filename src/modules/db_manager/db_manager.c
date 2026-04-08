#include "db_manager_internal.h"
#include "../../shared/module/module.h"
#include "../../shared/log/log.h"
#include <stdlib.h>
#include <string.h>

sqlite3 *g_db = NULL;
lv_mutex_t g_mutex;
int g_initialized = 0;

void db_copy_text(char *dest, size_t dest_size, const unsigned char *text) {
    if (!dest || dest_size == 0) {
        return;
    }

    if (text) {
        strncpy(dest, (const char *)text, dest_size - 1);
        dest[dest_size - 1] = '\0';
        return;
    }

    dest[0] = '\0';
}

void db_fill_video_info(sqlite3_stmt *stmt, VideoInfo *video) {
    if (!stmt || !video) {
        return;
    }

    video->id = sqlite3_column_int64(stmt, 0);
    db_copy_text(video->path, sizeof(video->path), sqlite3_column_text(stmt, 1));
    db_copy_text(video->title, sizeof(video->title), sqlite3_column_text(stmt, 2));
    db_copy_text(video->category, sizeof(video->category), sqlite3_column_text(stmt, 3));
    video->size = sqlite3_column_int64(stmt, 4);
    video->created_at = sqlite3_column_int64(stmt, 5);
}

void db_fill_history_info(sqlite3_stmt *stmt, HistoryInfo *history) {
    if (!stmt || !history) {
        return;
    }

    history->id = sqlite3_column_int64(stmt, 0);
    history->video_id = sqlite3_column_int64(stmt, 1);
    db_copy_text(history->video_title, sizeof(history->video_title), sqlite3_column_text(stmt, 2));
    db_copy_text(history->video_path, sizeof(history->video_path), sqlite3_column_text(stmt, 3));
    history->position = sqlite3_column_int64(stmt, 4);
    history->played_at = sqlite3_column_int64(stmt, 5);
}

void db_fill_favorite_info(sqlite3_stmt *stmt, FavoriteInfo *favorite) {
    if (!stmt || !favorite) {
        return;
    }

    favorite->id = sqlite3_column_int64(stmt, 0);
    favorite->video_id = sqlite3_column_int64(stmt, 1);
    db_copy_text(favorite->video_title, sizeof(favorite->video_title), sqlite3_column_text(stmt, 2));
    db_copy_text(favorite->video_path, sizeof(favorite->video_path), sqlite3_column_text(stmt, 3));
    favorite->created_at = sqlite3_column_int64(stmt, 4);
}

void db_fill_blacklist_info(sqlite3_stmt *stmt, BlacklistInfo *entry) {
    if (!stmt || !entry) {
        return;
    }

    entry->id = sqlite3_column_int64(stmt, 0);
    db_copy_text(entry->path, sizeof(entry->path), sqlite3_column_text(stmt, 1));
    entry->created_at = sqlite3_column_int64(stmt, 2);
}

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

    if (lv_mutex_init(&g_mutex) != LV_OK) {
        log_error("Failed to initialize mutex for db_manager");
        return;
    }

    g_initialized = 1;
    db_manager_init("./local_video.db");

    /* Log database video count for diagnostics */
    int64_t count = 0;
    db_manager_video_count(&count);
    log_info("Database initialized with %ld videos", (long)count);
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

    if (sqlite3_open(db_path, &g_db) != SQLITE_OK) {
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
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        log_error("SQL error: %s", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(g_db);
        g_db = NULL;
        lv_mutex_unlock(&g_mutex);
        return LV_ERROR_DB;
    }

    err_msg = NULL;
    rc = sqlite3_exec(g_db, "ALTER TABLE videos ADD COLUMN blacklisted INTEGER DEFAULT 0;", NULL, NULL, &err_msg);
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
