#ifndef DB_MANAGER_INTERNAL_H
#define DB_MANAGER_INTERNAL_H

#include "db_manager.h"
#include "../../shared/thread/thread.h"

extern sqlite3 *g_db;
extern lv_mutex_t g_mutex;
extern int g_initialized;

void db_copy_text(char *dest, size_t dest_size, const unsigned char *text);
void db_fill_video_info(sqlite3_stmt *stmt, VideoInfo *video);
void db_fill_history_info(sqlite3_stmt *stmt, HistoryInfo *history);
void db_fill_favorite_info(sqlite3_stmt *stmt, FavoriteInfo *favorite);
void db_fill_blacklist_info(sqlite3_stmt *stmt, BlacklistInfo *entry);

#endif
