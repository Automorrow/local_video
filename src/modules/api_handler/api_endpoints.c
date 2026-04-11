#include "api_handler_internal.h"
#include "../video_scanner/video_scanner.h"
#include "../config/config.h"
#include "../db_manager/db_manager.h"
#include "../../shared/log/log.h"
#include "../../include/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <strings.h>

#ifdef _WIN32
#define STRCASECMP _stricmp
#else
#define STRCASECMP strcasecmp
#include <dirent.h>
#include <sys/stat.h>
#endif

static int video_list_callback(const VideoInfo *video, void *user_data)
{
    response_buffer_t *buf = (response_buffer_t *)user_data;
    char tmp[64];

    if (buf->size > 1) {
        api_buffer_append_str(buf, ",");
    }

    api_buffer_append_str(buf, "{\"id\":");
    snprintf(tmp, sizeof(tmp), "%" PRId64, (int64_t)video->id);
    api_buffer_append_str(buf, tmp);
    api_buffer_append_str(buf, ",\"title\":\"");
    api_buffer_append_json_str(buf, video->title);
    api_buffer_append_str(buf, "\",\"path\":\"");
    api_buffer_append_json_str(buf, video->path);
    api_buffer_append_str(buf, "\",\"category\":\"");
    api_buffer_append_json_str(buf, video->category);
    api_buffer_append_str(buf, "\",\"size\":");
    snprintf(tmp, sizeof(tmp), "%" PRId64, (int64_t)video->size);
    api_buffer_append_str(buf, tmp);
    api_buffer_append_str(buf, "}");
    return 0;
}

static int history_callback(const HistoryInfo *history, void *user_data)
{
    response_buffer_t *buf = (response_buffer_t *)user_data;
    char tmp[64];

    if (buf->size > 1) {
        api_buffer_append_str(buf, ",");
    }

    api_buffer_append_str(buf, "{\"id\":");
    snprintf(tmp, sizeof(tmp), "%" PRId64, (int64_t)history->id);
    api_buffer_append_str(buf, tmp);
    api_buffer_append_str(buf, ",\"video_id\":");
    snprintf(tmp, sizeof(tmp), "%" PRId64, (int64_t)history->video_id);
    api_buffer_append_str(buf, tmp);
    api_buffer_append_str(buf, ",\"title\":\"");
    api_buffer_append_json_str(buf, history->video_title);
    api_buffer_append_str(buf, "\",\"path\":\"");
    api_buffer_append_json_str(buf, history->video_path);
    api_buffer_append_str(buf, "\",\"position\":");
    snprintf(tmp, sizeof(tmp), "%" PRId64, (int64_t)history->position);
    api_buffer_append_str(buf, tmp);
    api_buffer_append_str(buf, "}");
    return 0;
}

static int favorite_callback(const FavoriteInfo *favorite, void *user_data)
{
    response_buffer_t *buf = (response_buffer_t *)user_data;
    char tmp[64];

    if (buf->size > 1) {
        api_buffer_append_str(buf, ",");
    }

    api_buffer_append_str(buf, "{\"id\":");
    snprintf(tmp, sizeof(tmp), "%" PRId64, (int64_t)favorite->id);
    api_buffer_append_str(buf, tmp);
    api_buffer_append_str(buf, ",\"video_id\":");
    snprintf(tmp, sizeof(tmp), "%" PRId64, (int64_t)favorite->video_id);
    api_buffer_append_str(buf, tmp);
    api_buffer_append_str(buf, ",\"title\":\"");
    api_buffer_append_json_str(buf, favorite->video_title);
    api_buffer_append_str(buf, "\",\"path\":\"");
    api_buffer_append_json_str(buf, favorite->video_path);
    api_buffer_append_str(buf, "\"}");
    return 0;
}

static int blacklist_list_callback(const BlacklistInfo *entry, void *user_data)
{
    response_buffer_t *buf = (response_buffer_t *)user_data;
    char tmp[256];

    if (buf->size > 1) {
        api_buffer_append_str(buf, ",");
    }

    snprintf(tmp, sizeof(tmp), "{\"id\":%" PRId64 ",\"path\":\"", (int64_t)entry->id);
    api_buffer_append_str(buf, tmp);
    api_buffer_append_json_str(buf, entry->path);
    snprintf(tmp, sizeof(tmp), "\",\"created_at\":%" PRId64 "}", (int64_t)entry->created_at);
    api_buffer_append_str(buf, tmp);
    return 0;
}

static int category_list_callback(const char *category, void *user_data)
{
    response_buffer_t *buf = (response_buffer_t *)user_data;

    if (buf->size > 1) {
        api_buffer_append_str(buf, ",");
    }

    api_buffer_append_str(buf, "{\"name\":\"");
    api_buffer_append_json_str(buf, category);
    api_buffer_append_str(buf, "\"}");
    return 0;
}

lv_error_t api_get_videos(int client_fd, const char *query)
{
    response_buffer_t buf;
    int limit = 0;
    int offset = 0;
    char search_key[64] = {0};
    char search_value[256] = {0};
    lv_error_t err;

    api_buffer_init(&buf);
    api_buffer_append_str(&buf, "[");

    if (query) {
        char limit_str[16] = {0};
        char offset_str[16] = {0};

        api_get_query_param(query, "limit", limit_str, sizeof(limit_str));
        api_get_query_param(query, "offset", offset_str, sizeof(offset_str));
        if (limit_str[0]) {
            limit = atoi(limit_str);
        }
        if (offset_str[0]) {
            offset = atoi(offset_str);
        }
    }

    if (limit > 500) {
        free(buf.data);
        return api_send_json_response(client_fd, "{\"success\":false,\"error\":\"Limit exceeds maximum (500)\"}", 400);
    }
    if (limit < 0 || offset < 0) {
        free(buf.data);
        return api_send_json_response(client_fd, "{\"success\":false,\"error\":\"Invalid limit or offset\"}", 400);
    }

    if (query && !limit) {
        log_info("Parsing query: %s", query);
        api_parse_query(query, search_key, sizeof(search_key), search_value, sizeof(search_value));
        log_info("Search key: '%s', value: '%s'", search_key, search_value);
    } else if (query) {
        api_parse_query(query, search_key, sizeof(search_key), search_value, sizeof(search_value));
    }

    if (search_key[0] && search_value[0]) {
        if (strcmp(search_key, "search") == 0) {
            log_info("Searching videos for: '%s'", search_value);
            db_manager_video_search(search_value, video_list_callback, &buf);
        } else if (strcmp(search_key, "category") == 0) {
            log_info("Getting videos by category: '%s'", search_value);
            db_manager_video_get_by_category(search_value, video_list_callback, &buf);
        }
    } else if (limit > 0) {
        log_info("Getting videos with limit=%d offset=%d", limit, offset);
        db_manager_video_get_all_paginated(video_list_callback, &buf, limit, offset);
    } else {
        log_info("Getting all videos");
        db_manager_video_get_all(video_list_callback, &buf);
    }

    api_buffer_append_str(&buf, "]");
    err = api_send_json_response(client_fd, buf.data, 200);
    api_buffer_free(&buf);
    return err;
}

lv_error_t api_get_config(int client_fd)
{
    const lv_config_t *cfg = config_get();
    char response[1024];
    snprintf(response, sizeof(response),
        "{\"port\":%d,\"scan_directory\":\"",
        (int)cfg->http_port);

    /* Build JSON with escaped scan_directory */
    response_buffer_t buf;
    api_buffer_init(&buf);
    api_buffer_append_str(&buf, response);
    api_buffer_append_json_str(&buf, cfg->scan_directory);
    api_buffer_append_str(&buf, "\",\"video_count\":0}");

    lv_error_t err = api_send_json_response(client_fd, buf.data, 200);
    api_buffer_free(&buf);
    return err;
}

lv_error_t api_update_config(int client_fd, const char *body)
{
    if (!body) {
        return api_send_json_response(client_fd,
            "{\"success\":false,\"error\":\"Empty body\"}", 400);
    }

    /* Parse JSON manually (no json library for parsing) */
    char port_str[16] = {0};
    char dir_str[512] = {0};

    /* Extract "port" field */
    const char *port_key = strstr(body, "\"port\"");
    if (port_key) {
        const char *colon = strchr(port_key + 6, ':');
        if (colon) {
            int i = 0;
            const char *p = colon + 1;
            while (*p && *p != ',' && *p != '}' && i < 15) {
                port_str[i++] = *p++;
            }
            port_str[i] = '\0';
        }
    }

    /* Extract "scan_directory" field */
    const char *dir_key = strstr(body, "\"scan_directory\"");
    if (dir_key) {
        const char *colon = strchr(dir_key + 16, ':');
        if (colon) {
            const char *start = strchr(colon, '"');
            if (start) {
                start++;
                const char *end = strchr(start, '"');
                if (end) {
                    size_t len = (size_t)(end - start);
                    if (len >= sizeof(dir_str)) len = sizeof(dir_str) - 1;
                    /* JSON unescape: \\ -> \, \" -> ", \/ -> / */
                    size_t j = 0;
                    for (size_t i = 0; i < len && j < sizeof(dir_str) - 1; i++) {
                        if (start[i] == '\\' && i + 1 < len) {
                            i++;
                            dir_str[j++] = start[i];
                        } else {
                            dir_str[j++] = start[i];
                        }
                    }
                    dir_str[j] = '\0';
                }
            }
        }
    }

    lv_error_t err = LV_OK;

    if (port_str[0]) {
        int port = atoi(port_str);
        if (port > 0 && port <= 65535) {
            err = config_set_port((uint16_t)port);
            if (err != LV_OK) {
                return api_send_json_response(client_fd,
                    "{\"success\":false,\"error\":\"Invalid port\"}", 400);
            }
        }
    }

    if (dir_str[0]) {
        err = config_set_scan_directory(dir_str);
        if (err != LV_OK) {
            return api_send_json_response(client_fd,
                "{\"success\":false,\"error\":\"Invalid directory\"}", 400);
        }
    }

    return api_send_json_response(client_fd, "{\"success\":true}", 200);
}

/*
 * Resolve a directory by its name.
 * First tries the video database, then falls back to filesystem search.
 * POST body: JSON array, e.g. ["431960"]
 * Returns the full path, e.g. "E:\\SteamLibrary\\...\\431960"
 */
static void resolve_search_recursive(const char *base, const char *target,
    char *result, size_t result_size, int depth, int max_depth);

lv_error_t api_resolve_dir(int client_fd, const char *body)
{
    if (!body || body[0] != '[') {
        return api_send_json_response(client_fd,
            "{\"success\":false,\"error\":\"Invalid body\"}", 400);
    }

    /* Parse first directory name from JSON array */
    char dir_name[256] = {0};
    const char *p = body + 1;
    while (*p && *p != ']' && *p != ',') {
        if (*p == '"') {
            p++;
            const char *start = p;
            while (*p && *p != '"') p++;
            size_t len = (size_t)(p - start);
            if (len >= sizeof(dir_name)) len = sizeof(dir_name) - 1;
            memcpy(dir_name, start, len);
            dir_name[len] = '\0';
            break;
        }
        p++;
    }

    if (dir_name[0] == '\0') {
        return api_send_json_response(client_fd,
            "{\"success\":false,\"error\":\"Empty directory name\"}", 400);
    }

    char result_path[1024] = {0};

    /* Strategy 1: Search database */
    char video_path[512] = {0};
    if (db_manager_video_search_by_path_substr(dir_name, video_path, sizeof(video_path)) == LV_OK && video_path[0]) {
        const char *sep = strstr(video_path, dir_name);
        if (sep) {
            size_t prefix_len = (size_t)(sep - video_path) + strlen(dir_name);
            if (prefix_len >= sizeof(result_path)) prefix_len = sizeof(result_path) - 1;
            memcpy(result_path, video_path, prefix_len);
        }
    }

    /* Strategy 2: Search filesystem on all drives */
    if (!result_path[0]) {
#ifdef _WIN32
        DWORD drives = GetLogicalDrives();
        for (char drive = 'A'; drive <= 'Z' && !result_path[0]; drive++) {
            if (!(drives & (1 << (drive - 'A')))) continue;
            UINT type = GetDriveTypeA((char[]){drive, ':', '\\', 0});
            if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE &&
                type != DRIVE_REMOTE && type != DRIVE_RAMDISK) continue;
            /* Recursively search X:\ for a directory named dir_name (max depth 8) */
            char search_root[4] = {drive, ':', '\\', 0};
            resolve_search_recursive(search_root, dir_name, result_path, sizeof(result_path), 0, 8);
        }
#else
        resolve_search_recursive("/", dir_name, result_path, sizeof(result_path), 0, 8);
#endif
    }

    response_buffer_t buf;
    api_buffer_init(&buf);
    if (result_path[0]) {
        api_buffer_append_str(&buf, "{\"success\":true,\"path\":\"");
        api_buffer_append_json_str(&buf, result_path);
        api_buffer_append_str(&buf, "\"}");
    } else {
        api_buffer_append_str(&buf, "{\"success\":false,\"error\":\"Directory not found\"}");
    }
    lv_error_t err = api_send_json_response(client_fd, buf.data, 200);
    api_buffer_free(&buf);
    return err;
}

/* Recursively search for a directory by name (platform-independent) */
static void resolve_search_recursive(const char *base, const char *target,
    char *result, size_t result_size, int depth, int max_depth)
{
    if (result[0] || depth > max_depth) return;

#ifdef _WIN32
    WIN32_FIND_DATAW find_data;
    WCHAR search_path[1024];
    char search_utf8[1024];
    snprintf(search_utf8, sizeof(search_utf8), "%s\\*", base);
    MultiByteToWideChar(CP_UTF8, 0, search_utf8, -1, search_path, 1024);

    HANDLE hFind = FindFirstFileExW(search_path, FindExInfoBasic,
        &find_data, FindExSearchLimitToDirectories, NULL, 0);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) continue;
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) continue;
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;

        char utf8_name[256];
        WideCharToMultiByte(CP_UTF8, 0, find_data.cFileName, -1, utf8_name, sizeof(utf8_name), NULL, NULL);

        /* Check if this directory matches the target name */
        if (STRCASECMP(utf8_name, target) == 0) {
            snprintf(result, result_size, "%s\\%s", base, utf8_name);
            FindClose(hFind);
            return;
        }

        /* Recurse into subdirectories */
        char child_path[1024];
        snprintf(child_path, sizeof(child_path), "%s\\%s", base, utf8_name);
        resolve_search_recursive(child_path, target, result, result_size, depth + 1, max_depth);
        if (result[0]) { FindClose(hFind); return; }
    } while (FindNextFileW(hFind, &find_data));
    FindClose(hFind);
#else
    DIR *dir = opendir(base);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char child_path[1024];
        snprintf(child_path, sizeof(child_path), "%s/%s", base, entry->d_name);
        struct stat st;
        if (stat(child_path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        if (strcmp(entry->d_name, target) == 0) {
            snprintf(result, result_size, "%s", child_path);
            closedir(dir);
            return;
        }
        resolve_search_recursive(child_path, target, result, result_size, depth + 1, max_depth);
        if (result[0]) { closedir(dir); return; }
    }
    closedir(dir);
#endif
}

#ifdef _WIN32
#include <windows.h>

lv_error_t api_browse_directories(int client_fd, const char *query)
{
    char path_buf[512] = {0};

    /* Extract path from query parameter */
    if (query) {
        const char *path_key = strstr(query, "path=");
        if (path_key) {
            const char *val = path_key + 5;
            /* URL-decode %XX */
            size_t j = 0;
            for (size_t i = 0; val[i] && val[i] != '&' && j < sizeof(path_buf) - 1; i++) {
                if (val[i] == '%' && val[i+1] && val[i+2]) {
                    char hex[3] = { val[i+1], val[i+2], 0 };
                    path_buf[j++] = (char)strtol(hex, NULL, 16);
                    i += 2;
                } else if (val[i] == '+') {
                    path_buf[j++] = ' ';
                } else {
                    path_buf[j++] = val[i];
                }
            }
        }
    }

    response_buffer_t buf;
    api_buffer_init(&buf);
    api_buffer_append_str(&buf, "[");

    WIN32_FIND_DATAW find_data;
    WCHAR search_path[1024];
    int first = 1;

    if (path_buf[0] == '\0') {
        /* List drives */
        DWORD drives = GetLogicalDrives();
        WCHAR drive[] = L"A:\\";
        for (char i = 0; i < 26; i++) {
            if (drives & (1 << i)) {
                drive[0] = L'A' + i;
                UINT type = GetDriveTypeW(drive);
                if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE ||
                    type == DRIVE_REMOTE || type == DRIVE_RAMDISK) {
                    if (!first) api_buffer_append_str(&buf, ",");
                    first = 0;
                    api_buffer_append_str(&buf, "{\"name\":\"");
                    char letter[4] = { 'A' + i, ':', '\\', 0 };
                    api_buffer_append_json_str(&buf, letter);
                    api_buffer_append_str(&buf, "\",\"type\":\"drive\"}");
                }
            }
        }
    } else {
        /* List subdirectories of path_buf */
        char search_utf8[1024];
        snprintf(search_utf8, sizeof(search_utf8), "%s\\*", path_buf);
        /* Convert to wide char */
        MultiByteToWideChar(CP_UTF8, 0, search_utf8, -1, search_path, 1024);

        HANDLE hFind = FindFirstFileExW(
            search_path,
            FindExInfoBasic,        /* No alternate file name */
            &find_data,
            FindExSearchLimitToDirectories,  /* Only directories */
            NULL,
            0
        );
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(find_data.cFileName, L".") == 0 ||
                    wcscmp(find_data.cFileName, L"..") == 0) continue;
                if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) continue;

                char utf8_name[512];
                WideCharToMultiByte(CP_UTF8, 0, find_data.cFileName, -1,
                                   utf8_name, sizeof(utf8_name), NULL, NULL);

                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s/%s", path_buf, utf8_name);
                /* Normalize to backslash */
                for (char *p = full_path; *p; p++) {
                    if (*p == '/') *p = '\\';
                }

                if (!first) api_buffer_append_str(&buf, ",");
                first = 0;
                api_buffer_append_str(&buf, "{\"name\":\"");
                api_buffer_append_json_str(&buf, utf8_name);
                api_buffer_append_str(&buf, "\",\"path\":\"");
                api_buffer_append_json_str(&buf, full_path);
                api_buffer_append_str(&buf, "\"}");
            } while (FindNextFileW(hFind, &find_data));
            FindClose(hFind);
        }
    }

    api_buffer_append_str(&buf, "]");
    lv_error_t err = api_send_json_response(client_fd, buf.data, 200);
    api_buffer_free(&buf);
    return err;
}

#else /* POSIX */

lv_error_t api_browse_directories(int client_fd, const char *query)
{
    char path_buf[512] = "/";

    if (query) {
        const char *path_key = strstr(query, "path=");
        if (path_key) {
            const char *val = path_key + 5;
            size_t j = 0;
            for (size_t i = 0; val[i] && val[i] != '&' && j < sizeof(path_buf) - 1; i++) {
                if (val[i] == '%' && val[i+1] && val[i+2]) {
                    char hex[3] = { val[i+1], val[i+2], 0 };
                    path_buf[j++] = (char)strtol(hex, NULL, 16);
                    i += 2;
                } else if (val[i] == '+') {
                    path_buf[j++] = ' ';
                } else {
                    path_buf[j++] = val[i];
                }
            }
        }
    }

    response_buffer_t buf;
    api_buffer_init(&buf);
    api_buffer_append_str(&buf, "[");

    DIR *dir = opendir(path_buf);
    if (dir) {
        struct dirent *entry;
        int first = 1;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", path_buf, entry->d_name);

            struct stat st;
            if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (!first) api_buffer_append_str(&buf, ",");
                first = 0;
                api_buffer_append_str(&buf, "{\"name\":\"");
                api_buffer_append_json_str(&buf, entry->d_name);
                api_buffer_append_str(&buf, "\",\"path\":\"");
                api_buffer_append_json_str(&buf, full_path);
                api_buffer_append_str(&buf, "\"}");
            }
        }
        closedir(dir);
    }

    api_buffer_append_str(&buf, "]");
    lv_error_t err = api_send_json_response(client_fd, buf.data, 200);
    api_buffer_free(&buf);
    return err;
}

#endif

lv_error_t api_get_random(int client_fd)
{
    response_buffer_t buf;
    lv_error_t err;

    api_buffer_init(&buf);
    api_buffer_append_str(&buf, "[");
    db_manager_video_get_random(1, video_list_callback, &buf);
    api_buffer_append_str(&buf, "]");

    err = api_send_json_response(client_fd, buf.data, 200);
    api_buffer_free(&buf);
    return err;
}

lv_error_t api_get_history(int client_fd)
{
    response_buffer_t buf;
    lv_error_t err;

    api_buffer_init(&buf);
    api_buffer_append_str(&buf, "[");
    db_manager_history_get(history_callback, &buf);
    api_buffer_append_str(&buf, "]");

    err = api_send_json_response(client_fd, buf.data, 200);
    api_buffer_free(&buf);
    return err;
}

lv_error_t api_add_history(int client_fd, const char *body)
{
    int64_t video_id = 0;
    int64_t position = 0;

    if (body) {
        sscanf(body, "{\"video_id\":%" SCNd64 ",\"position\":%" SCNd64 "}", &video_id, &position);
    }

    if (video_id > 0) {
        db_manager_history_add(video_id, position);
    }

    return api_send_json_response(client_fd, "{\"success\":true}", 200);
}

lv_error_t api_delete_history(int client_fd, const char *path)
{
    int64_t id = 0;
    lv_error_t err;

    if (path) {
        const char *id_start = strrchr(path, '/');
        if (id_start) {
            id_start++;
            id = strtoll(id_start, NULL, 10);
        }
    }

    if (id <= 0) {
        return api_send_json_response(client_fd, "{\"success\":false,\"error\":\"Invalid id\"}", 400);
    }

    err = db_manager_history_delete(id);
    if (err == LV_OK) {
        return api_send_json_response(client_fd, "{\"success\":true}", 200);
    }

    return api_send_json_response(client_fd, "{\"success\":false,\"error\":\"Failed to delete\"}", 500);
}

lv_error_t api_clear_history(int client_fd)
{
    db_manager_history_clear();
    return api_send_json_response(client_fd, "{\"success\":true}", 200);
}

lv_error_t api_get_favorites(int client_fd)
{
    response_buffer_t buf;
    lv_error_t err;

    api_buffer_init(&buf);
    api_buffer_append_str(&buf, "[");
    db_manager_favorites_list(favorite_callback, &buf);
    api_buffer_append_str(&buf, "]");

    err = api_send_json_response(client_fd, buf.data, 200);
    api_buffer_free(&buf);
    return err;
}

lv_error_t api_add_favorite(int client_fd, const char *body)
{
    int64_t video_id = 0;

    if (body) {
        sscanf(body, "{\"video_id\":%" SCNd64 "}", &video_id);
    }

    if (video_id > 0) {
        db_manager_favorite_add(video_id);
    }

    return api_send_json_response(client_fd, "{\"success\":true}", 200);
}

lv_error_t api_remove_favorite(int client_fd, const char *path)
{
    int64_t video_id = 0;

    if (path) {
        sscanf(path, "/api/favorites/%" SCNd64, &video_id);
    }

    if (video_id > 0) {
        db_manager_favorite_remove(video_id);
    }

    return api_send_json_response(client_fd, "{\"success\":true}", 200);
}

lv_error_t api_get_blacklist(int client_fd)
{
    response_buffer_t buf;
    lv_error_t err;

    api_buffer_init(&buf);
    api_buffer_append_str(&buf, "[");
    db_manager_blacklist_get_all(blacklist_list_callback, &buf);
    api_buffer_append_str(&buf, "]");

    err = api_send_json_response(client_fd, buf.data, 200);
    api_buffer_free(&buf);
    return err;
}

lv_error_t api_add_blacklist(int client_fd, const char *body)
{
    char path[512] = {0};
    int64_t new_id = 0;
    lv_error_t err;

    if (body) {
        const char *path_start = strstr(body, "\"path\":\"");
        if (path_start) {
            const char *path_end;
            size_t len;

            path_start += 8;
            path_end = strchr(path_start, '"');
            if (path_end) {
                len = (size_t)(path_end - path_start);
                if (len < sizeof(path)) {
                    memcpy(path, path_start, len);
                    path[len] = '\0';
                }
            }
        }
    }

    if (strlen(path) == 0) {
        return api_send_json_response(client_fd, "{\"success\":false,\"error\":\"Invalid path\"}", 400);
    }
    if (access(path, F_OK) != 0) {
        return api_send_json_response(client_fd, "{\"success\":false,\"error\":\"Directory does not exist\"}", 400);
    }

    err = db_manager_blacklist_add(path, &new_id);
    if (err == LV_OK) {
        char response[512];

        db_manager_video_blacklist_by_path_prefix(path);
        snprintf(response, sizeof(response), "{\"success\":true,\"data\":{\"id\":%" PRId64 "}}", (int64_t)new_id);
        return api_send_json_response(client_fd, response, 200);
    }

    return api_send_json_response(client_fd, "{\"success\":false,\"error\":\"Directory already in blacklist\"}", 409);
}

lv_error_t api_remove_blacklist(int client_fd, const char *path)
{
    int64_t id = 0;

    if (path) {
        const char *id_start = strrchr(path, '/');
        if (id_start) {
            id_start++;
            id = strtoll(id_start, NULL, 10);
        }
    }

    if (id > 0) {
        char blacklist_path[512] = {0};

        db_manager_blacklist_get_by_id(id, blacklist_path, sizeof(blacklist_path));
        db_manager_blacklist_delete(id);

        if (blacklist_path[0] != '\0') {
            log_info("Blacklist entry removed: %s", blacklist_path);
            log_info("Unblacklisting videos...");
            db_manager_video_unblacklist_by_path_prefix(blacklist_path);
            video_scanner_scan(blacklist_path);
        } else {
            db_manager_video_unblacklist_all();
        }

        return api_send_json_response(client_fd, "{\"success\":true,\"message\":\"Directory removed from blacklist and videos are being restored\"}", 200);
    }

    return api_send_json_response(client_fd, "{\"success\":false,\"error\":\"Blacklist entry not found\"}", 404);
}

lv_error_t api_get_categories(int client_fd)
{
    response_buffer_t buf;
    lv_error_t err;

    api_buffer_init(&buf);
    api_buffer_append_str(&buf, "[");
    db_manager_category_get_all(category_list_callback, &buf);
    api_buffer_append_str(&buf, "]");

    err = api_send_json_response(client_fd, buf.data, 200);
    api_buffer_free(&buf);
    return err;
}
