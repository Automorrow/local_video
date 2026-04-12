#include "db_manager_internal.h"
#include "../../shared/log/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

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

lv_error_t db_manager_video_search_by_path_substr(const char *substr, char *out_path, size_t out_size) {
    if (!substr || !out_path) return LV_ERROR_INVALID_ARG;
    lv_error_t err = LV_OK;
    lv_mutex_lock(&g_mutex);
    const char *sql = "SELECT path FROM videos WHERE path LIKE ? LIMIT 1";
    sqlite3_stmt *stmt = NULL;
    char pattern[600];
    snprintf(pattern, sizeof(pattern), "%%%s%%", substr);
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *path = (const char *)sqlite3_column_text(stmt, 0);
            if (path) {
                strncpy(out_path, path, out_size - 1);
                out_path[out_size - 1] = '\0';
            }
        } else {
            err = LV_ERROR_DB;
        }
        sqlite3_finalize(stmt);
    } else {
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
    const char *sql = "SELECT id, path, title, category, size, created_at, play_count FROM videos WHERE blacklisted = 0 ORDER BY id DESC";
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

    const char *sql = "SELECT id, path, title, category, size, created_at, play_count FROM videos WHERE blacklisted = 0 ORDER BY id DESC LIMIT ? OFFSET ?";
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
    const char *sql = "SELECT id, path, title, category, size, created_at, play_count, blacklisted FROM videos WHERE id = ?";
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
        if (sqlite3_column_int(stmt, 7) != 0) {
            log_warning("Video ID %" PRId64 " is blacklisted", id);
            err = LV_ERROR_DB;
        } else {
            db_fill_video_info(stmt, out);
        }
    } else {
        log_warning("Video ID %" PRId64 " not found in database (rc=%d)", id, rc);
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
    const char *sql = "SELECT id, path, title, category, size, created_at, play_count FROM videos "
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
    const char *sql = "SELECT id, path, title, category, size, created_at, play_count FROM videos WHERE category = ? ORDER BY id DESC";
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

static int pick_weighted_random_bucket(int *counts, const int *weights, int num_buckets)
{
    int total_weight = 0;
    for (int i = 0; i < num_buckets; i++) {
        if (counts[i] > 0) total_weight += weights[i];
    }
    if (total_weight <= 0) return -1;

    int r = rand() % total_weight;
    for (int i = 0; i < num_buckets; i++) {
        if (counts[i] > 0) {
            r -= weights[i];
            if (r < 0) return i;
        }
    }
    return -1;
}

lv_error_t db_manager_video_get_random(int count, int64_t exclude_video_id, int exclude_recent_count, video_callback_t callback, void *user_data) {
    if (!callback || count <= 0) {
        return LV_ERROR_INVALID_ARG;
    }

    /* Cap count to avoid excessive stack usage */
    if (count > 64) count = 64;

    int64_t selected_ids[64];
    int selected_count = 0;
    lv_error_t err = LV_OK;

    const char *play_conditions[4] = {
        "play_count = 0",
        "play_count BETWEEN 1 AND 2",
        "play_count BETWEEN 3 AND 5",
        "play_count > 5"
    };
    const int bucket_weights[4] = {40, 30, 20, 10};

    lv_mutex_lock(&g_mutex);

    for (int round = 0; round < 2 && selected_count < count; round++) {
        /* round 0: strict filters; round 1: fallback excluding only exclude_video_id */
        int use_recent = (round == 0 && exclude_recent_count > 0);

        int retries = 0;
        while (selected_count < count && retries < 20) {
            int bucket_counts[4] = {0, 0, 0, 0};

            /* Count eligible videos in each bucket */
            for (int b = 0; b < 4; b++) {
                char count_sql[512];
                snprintf(count_sql, sizeof(count_sql),
                    "SELECT COUNT(*) FROM videos WHERE blacklisted = 0 %s%s AND %s",
                    (exclude_video_id > 0) ? "AND id != ?" : "",
                    use_recent ? " AND id NOT IN (SELECT video_id FROM history ORDER BY played_at DESC LIMIT ?)" : "",
                    play_conditions[b]);

                sqlite3_stmt *stmt = NULL;
                if (sqlite3_prepare_v2(g_db, count_sql, -1, &stmt, NULL) != SQLITE_OK) continue;

                int param = 1;
                if (exclude_video_id > 0) {
                    sqlite3_bind_int64(stmt, param++, exclude_video_id);
                }
                if (use_recent) {
                    sqlite3_bind_int(stmt, param++, exclude_recent_count);
                }

                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    bucket_counts[b] = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);

                /* Subtract already-selected videos from this bucket count */
                for (int s = 0; s < selected_count; s++) {
                    sqlite3_stmt *check_stmt = NULL;
                    const char *check_sql = "SELECT 1 FROM videos WHERE id = ? AND blacklisted = 0 AND ?";
                    if (sqlite3_prepare_v2(g_db, check_sql, -1, &check_stmt, NULL) == SQLITE_OK) {
                        sqlite3_bind_int64(check_stmt, 1, selected_ids[s]);
                        sqlite3_bind_text(check_stmt, 2, play_conditions[b], -1, SQLITE_TRANSIENT);
                        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
                            bucket_counts[b]--;
                        }
                        sqlite3_finalize(check_stmt);
                    }
                }
                if (bucket_counts[b] < 0) bucket_counts[b] = 0;
            }

            int bucket = pick_weighted_random_bucket(bucket_counts, bucket_weights, 4);
            if (bucket < 0) {
                retries++;
                continue; /* No eligible videos in this round */
            }

            /* Pick a random offset within the chosen bucket */
            int offset = rand() % bucket_counts[bucket];
            char select_sql[512];
            snprintf(select_sql, sizeof(select_sql),
                "SELECT id, path, title, category, size, created_at, play_count FROM videos WHERE blacklisted = 0 %s%s AND %s LIMIT 1 OFFSET ?",
                (exclude_video_id > 0) ? "AND id != ?" : "",
                use_recent ? " AND id NOT IN (SELECT video_id FROM history ORDER BY played_at DESC LIMIT ?)" : "",
                play_conditions[bucket]);

            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(g_db, select_sql, -1, &stmt, NULL) != SQLITE_OK) break;

            int param = 1;
            if (exclude_video_id > 0) {
                sqlite3_bind_int64(stmt, param++, exclude_video_id);
            }
            if (use_recent) {
                sqlite3_bind_int(stmt, param++, exclude_recent_count);
            }
            sqlite3_bind_int(stmt, param++, offset);

            VideoInfo video;
            int found = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                db_fill_video_info(stmt, &video);
                found = 1;
            }
            sqlite3_finalize(stmt);

            if (!found) {
                retries++;
                continue;
            }

            /* Check for duplicates */
            int duplicate = 0;
            for (int s = 0; s < selected_count; s++) {
                if (selected_ids[s] == video.id) {
                    duplicate = 1;
                    break;
                }
            }
            if (duplicate) {
                /* If duplicate, try again in same round */
                retries++;
                continue;
            }

            retries = 0;
            selected_ids[selected_count++] = video.id;
            if (callback(&video, user_data) != 0) {
                selected_count = count; /* abort */
                break;
            }
        }
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
