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

lv_error_t api_get_scan_status(int client_fd)
{
    int scanning = video_scanner_is_scanning();
    int64_t video_count = 0;
    db_manager_video_count(&video_count);

    char response[256];
    snprintf(response, sizeof(response),
        "{\"scanning\":%s,\"video_count\":%" PRId64 "}",
        scanning ? "true" : "false",
        video_count);

    return api_send_json_response(client_fd, response, 200);
}

#ifdef _WIN32
#define STRCASECMP _stricmp
#else
#define STRCASECMP strcasecmp
#include <dirent.h>
#include <sys/stat.h>
#endif

static void resolve_search_recursive(const char *base, const char *target,
    char *result, size_t result_size,
    char children[][256], int child_count,
    int depth, int max_depth);
static int resolve_verify_children(const char *dir_path, char children[][256], int child_count);

typedef struct {
    response_buffer_t *buf;
    int first;
} browse_db_ctx_t;

static void normalize_slashes(char *path)
{
    for (char *p = path; *p; p++) {
        if (*p == '\\') *p = '/';
    }
}

static int browse_db_callback(const char *name, const char *path, void *user_data)
{
    browse_db_ctx_t *ctx = (browse_db_ctx_t *)user_data;
    if (!ctx->first) {
        api_buffer_append_str(ctx->buf, ",");
    }
    ctx->first = 0;

    char path_copy[1024];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
#ifdef _WIN32
    for (char *p = path_copy; *p; p++) {
        if (*p == '/') *p = '\\';
    }
#endif

    api_buffer_append_str(ctx->buf, "{\"name\":\"");
    api_buffer_append_json_str(ctx->buf, name);
    api_buffer_append_str(ctx->buf, "\",\"path\":\"");
    api_buffer_append_json_str(ctx->buf, path_copy);
    api_buffer_append_str(ctx->buf, "\"}");
    return 0;
}

static lv_error_t try_browse_from_db(int client_fd, const char *parent_path)
{
    response_buffer_t buf;
    api_buffer_init(&buf);
    api_buffer_append_str(&buf, "[");

    browse_db_ctx_t ctx = { &buf, 1 };
    lv_error_t err = db_manager_directory_get_children(parent_path, browse_db_callback, &ctx);

    if (err == LV_OK && buf.size > 1) {
        api_buffer_append_str(&buf, "]");
        err = api_send_json_response(client_fd, buf.data, 200);
        api_buffer_free(&buf);
        return err;
    }

    api_buffer_free(&buf);
    return LV_ERROR_UNKNOWN;
}

static const char *json_find_key_colon(const char *body, const char *key)
{
    size_t key_len = strlen(key);
    char pattern[64];
    if (key_len >= sizeof(pattern) - 3) return NULL;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = body;
    while ((p = strstr(p, pattern)) != NULL) {
        if (p > body) {
            char prev = *(p - 1);
            if (prev == '"' || (prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z') || prev == '_' || (prev >= '0' && prev <= '9')) {
                p += 1;
                continue;
            }
        }
        const char *q = p + strlen(pattern);
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
        if (*q == ':') {
            return q;
        }
        p += 1;
    }
    return NULL;
}

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

    {
        char search_buf[256] = {0};
        char category_buf[256] = {0};
        if (query) {
            api_get_query_param(query, "search", search_buf, sizeof(search_buf));
            api_get_query_param(query, "category", category_buf, sizeof(category_buf));
        }
        if (search_buf[0]) {
            snprintf(search_value, sizeof(search_value), "%s", search_buf);
            log_info("Searching videos for: '%s'", search_value);
            db_manager_video_search(search_value, video_list_callback, &buf);
        } else if (category_buf[0]) {
            snprintf(search_value, sizeof(search_value), "%s", category_buf);
            log_info("Getting videos by category: '%s'", search_value);
            db_manager_video_get_by_category(search_value, video_list_callback, &buf);
        } else if (limit > 0) {
            log_info("Getting videos with limit=%d offset=%d", limit, offset);
            db_manager_video_get_all_paginated(video_list_callback, &buf, limit, offset);
        } else {
            log_info("Getting all videos");
            db_manager_video_get_all(video_list_callback, &buf);
        }
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
    const char *port_colon = json_find_key_colon(body, "port");
    if (port_colon) {
        int i = 0;
        const char *p = port_colon + 1;
        while (*p && *p != ',' && *p != '}' && i < 15) {
            port_str[i++] = *p++;
        }
        port_str[i] = '\0';
    }

    /* Extract "scan_directory" field */
    const char *dir_colon = json_find_key_colon(body, "scan_directory");
    if (dir_colon) {
        const char *start = strchr(dir_colon, '"');
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
        /* Trigger video scan for the new directory */
        video_scanner_scan(dir_str);
    }

    return api_send_json_response(client_fd, "{\"success\":true}", 200);
}

/*
 * Resolve a directory by its name.
 * First tries the video database, then falls back to filesystem search.
 * POST body: JSON array, e.g. ["431960"]
 * Returns the full path, e.g. "E:\\SteamLibrary\\...\\431960"
 */
/*
 * Resolve a directory by its name and children.
 * POST body: {"name":"431960","children":["subdir1","video1.mp4",...]}
 * Strategy 1: Search database for a video path containing dir_name
 * Strategy 2: Recursively search filesystem, verify by checking children exist
 */
lv_error_t api_resolve_dir(int client_fd, const char *body)
{
    if (!body || body[0] != '{') {
        return api_send_json_response(client_fd,
            "{\"success\":false,\"error\":\"Invalid body\"}", 400);
    }

    /* Parse "name" field */
    char dir_name[256] = {0};
    const char *name_colon = json_find_key_colon(body, "name");
    if (name_colon) {
        const char *start = strchr(name_colon, '"');
        if (start) {
            start++;
            const char *end = strchr(start, '"');
            if (end) {
                size_t len = (size_t)(end - start);
                if (len >= sizeof(dir_name)) len = sizeof(dir_name) - 1;
                memcpy(dir_name, start, len);
                dir_name[len] = '\0';
            }
        }
    }

    /* Parse "children" array */
    char children[32][256];
    int child_count = 0;
    const char *children_colon = json_find_key_colon(body, "children");
    if (children_colon) {
        const char *arr_start = strchr(children_colon, '[');
        if (arr_start) {
            const char *q = arr_start + 1;
            while (*q && *q != ']' && child_count < 32) {
                if (*q == '"') {
                    q++;
                    const char *s = q;
                    while (*q && *q != '"') q++;
                    size_t len = (size_t)(q - s);
                    if (len >= 256) len = 255;
                    memcpy(children[child_count], s, len);
                    children[child_count][len] = '\0';
                    child_count++;
                    if (*q == '"') q++;
                } else {
                    q++;
                }
            }
        }
    }

    if (dir_name[0] == '\0') {
        return api_send_json_response(client_fd,
            "{\"success\":false,\"error\":\"Empty directory name\"}", 400);
    }

    char result_path[1024] = {0};
    const lv_config_t *cfg = config_get();

    /* Strategy 1: Search database */
    char video_path[512] = {0};
    if (db_manager_video_search_by_path_substr(dir_name, video_path, sizeof(video_path)) == LV_OK && video_path[0]) {
        const char *sep = strstr(video_path, dir_name);
        if (sep) {
            size_t prefix_len = (size_t)(sep - video_path) + strlen(dir_name);
            if (prefix_len >= sizeof(result_path)) prefix_len = sizeof(result_path) - 1;
            memcpy(result_path, video_path, prefix_len);
            if (!config_path_allowed(result_path)) {
                result_path[0] = '\0';
            }
        }
    }

    /* Strategy 2: Search filesystem within scan_directory, verify with children */
    if (!result_path[0] && cfg->scan_directory && cfg->scan_directory[0]) {
#ifdef _WIN32
        resolve_search_recursive(cfg->scan_directory, dir_name, result_path, sizeof(result_path),
            children, child_count, 0, 8);
#else
        resolve_search_recursive(cfg->scan_directory, dir_name, result_path, sizeof(result_path),
            children, child_count, 0, 8);
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

/* Recursively search for a directory by name, verify with children */
static void resolve_search_recursive(const char *base, const char *target,
    char *result, size_t result_size,
    char children[][256], int child_count,
    int depth, int max_depth)
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

        if (STRCASECMP(utf8_name, target) == 0) {
            /* Found matching directory name - verify with children */
            char candidate[1024];
            snprintf(candidate, sizeof(candidate), "%s\\%s", base, utf8_name);
            if (child_count > 0 && resolve_verify_children(candidate, children, child_count)) {
                snprintf(result, result_size, "%s", candidate);
                FindClose(hFind);
                return;
            } else if (child_count == 0) {
                snprintf(result, result_size, "%s", candidate);
                FindClose(hFind);
                return;
            }
        }

        char child_path[1024];
        snprintf(child_path, sizeof(child_path), "%s\\%s", base, utf8_name);
        resolve_search_recursive(child_path, target, result, result_size,
            children, child_count, depth + 1, max_depth);
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
            if (child_count > 0 && resolve_verify_children(child_path, children, child_count)) {
                snprintf(result, result_size, "%s", child_path);
                closedir(dir);
                return;
            } else if (child_count == 0) {
                snprintf(result, result_size, "%s", child_path);
                closedir(dir);
                return;
            }
        }
        resolve_search_recursive(child_path, target, result, result_size,
            children, child_count, depth + 1, max_depth);
        if (result[0]) { closedir(dir); return; }
    }
    closedir(dir);
#endif
}

/* Verify that a directory contains at least one of the expected children */
static int resolve_verify_children(const char *dir_path, char children[][256], int child_count)
{
    if (child_count == 0) return 1;
    int matches = 0;
    /* Only need to match a few children to be confident (avoid slow full enumeration) */
    int needed = child_count > 3 ? 3 : child_count;

#ifdef _WIN32
    WIN32_FIND_DATAW find_data;
    WCHAR search_path[1024];
    char search_utf8[1024];
    snprintf(search_utf8, sizeof(search_utf8), "%s\\*", dir_path);
    MultiByteToWideChar(CP_UTF8, 0, search_utf8, -1, search_path, 1024);

    HANDLE hFind = FindFirstFileW(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    do {
        char utf8_name[256];
        WideCharToMultiByte(CP_UTF8, 0, find_data.cFileName, -1, utf8_name, sizeof(utf8_name), NULL, NULL);
        for (int i = 0; i < child_count; i++) {
            if (STRCASECMP(utf8_name, children[i]) == 0) {
                matches++;
                if (matches >= needed) { FindClose(hFind); return 1; }
                break;
            }
        }
    } while (FindNextFileW(hFind, &find_data));
    FindClose(hFind);
#else
    DIR *dir = opendir(dir_path);
    if (!dir) return 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        for (int i = 0; i < child_count; i++) {
            if (strcmp(entry->d_name, children[i]) == 0) {
                matches++;
                if (matches >= needed) { closedir(dir); return 1; }
                break;
            }
        }
    }
    closedir(dir);
#endif
    return matches >= needed;
}

#ifdef _WIN32
#include <windows.h>

lv_error_t api_browse_directories(int client_fd, const char *query)
{
    char path_buf[512] = {0};
    (void)config_get(); /* cfg not needed in browse on Windows */

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
                    char letter[4] = { 'A' + i, ':', '\\', 0 };
                    api_buffer_append_str(&buf, "{\"name\":\"");
                    api_buffer_append_json_str(&buf, letter);
                    api_buffer_append_str(&buf, "\",\"path\":\"");
                    api_buffer_append_json_str(&buf, letter);
                    api_buffer_append_str(&buf, "\",\"type\":\"drive\"}");
                }
            }
        }
    } else {
        /* Try database cache first */
        char db_query_path[512];
        strncpy(db_query_path, path_buf, sizeof(db_query_path) - 1);
        db_query_path[sizeof(db_query_path) - 1] = '\0';
        normalize_slashes(db_query_path);
        if (try_browse_from_db(client_fd, db_query_path) == LV_OK) {
            api_buffer_free(&buf);
            return LV_OK;
        }

        /* List subdirectories of path_buf */
        char search_utf8[1024];
        snprintf(search_utf8, sizeof(search_utf8), "%s\\*", path_buf);
        /* Convert to wide char */
        MultiByteToWideChar(CP_UTF8, 0, search_utf8, -1, search_path, 1024);

        /* FindExSearchLimitToDirectories is unreliable on drive roots.
         * Use FindFirstFileW with a scan limit to avoid hanging on large dirs. */
        HANDLE hFind = FindFirstFileW(search_path, &find_data);
        int dir_count = 0;
        int scan_count = 0;
        #define MAX_SCAN_ENTRIES 10000
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                scan_count++;
                if (scan_count > MAX_SCAN_ENTRIES) break;
                if (wcscmp(find_data.cFileName, L".") == 0 ||
                    wcscmp(find_data.cFileName, L"..") == 0) continue;
                if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) continue;
                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) continue;

                char utf8_name[512];
                WideCharToMultiByte(CP_UTF8, 0, find_data.cFileName, -1,
                                   utf8_name, sizeof(utf8_name), NULL, NULL);

                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s/%s", path_buf, utf8_name);
                db_manager_directory_upsert(full_path, utf8_name, path_buf);
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
                dir_count++;
                if (dir_count >= 500) break;  /* Limit to prevent huge responses */
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
    char path_buf[512] = {0};
    const lv_config_t *cfg = config_get();

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

    if (path_buf[0] == '\0') {
        if (cfg->scan_directory && cfg->scan_directory[0]) {
            strncpy(path_buf, cfg->scan_directory, sizeof(path_buf) - 1);
            path_buf[sizeof(path_buf) - 1] = '\0';
        } else {
            path_buf[0] = '/';
            path_buf[1] = '\0';
        }
    }

    response_buffer_t buf;
    api_buffer_init(&buf);
    api_buffer_append_str(&buf, "[");

    /* Try database cache first */
    char db_query_path[512];
    strncpy(db_query_path, path_buf, sizeof(db_query_path) - 1);
    db_query_path[sizeof(db_query_path) - 1] = '\0';
    normalize_slashes(db_query_path);
    if (try_browse_from_db(client_fd, db_query_path) == LV_OK) {
        api_buffer_free(&buf);
        return LV_OK;
    }

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
                db_manager_directory_upsert(full_path, entry->d_name, path_buf);
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

lv_error_t api_get_random(int client_fd, const char *query)
{
    response_buffer_t buf;
    lv_error_t err;

    int64_t exclude_id = 0;
    if (query) {
        const char *p = strstr(query, "exclude=");
        if (p) {
            exclude_id = (int64_t)atoll(p + 8);
        }
    }

    api_buffer_init(&buf);
    api_buffer_append_str(&buf, "[");
    db_manager_video_get_random(1, exclude_id, 10, video_list_callback, &buf);
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
        if (sscanf(body, "{\"video_id\":%" SCNd64 ",\"position\":%" SCNd64 "}", &video_id, &position) != 2) {
            return api_send_json_response(client_fd, "{\"success\":false,\"error\":\"Invalid body\"}", 400);
        }
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
        if (sscanf(body, "{\"video_id\":%" SCNd64 "}", &video_id) != 1) {
            return api_send_json_response(client_fd, "{\"success\":false,\"error\":\"Invalid body\"}", 400);
        }
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
        if (sscanf(path, "/api/favorites/%" SCNd64, &video_id) != 1) {
            return api_send_json_response(client_fd, "{\"success\":false,\"error\":\"Invalid id\"}", 400);
        }
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
        const char *path_colon = json_find_key_colon(body, "path");
        if (path_colon) {
            const char *path_start = strchr(path_colon, '"');
            if (path_start) {
                const char *path_end;
                size_t len;
                path_start++;
                path_end = strchr(path_start, '"');
                if (path_end) {
                    len = (size_t)(path_end - path_start);
                    if (len >= sizeof(path)) len = sizeof(path) - 1;
                    memcpy(path, path_start, len);
                    path[len] = '\0';
                }
            }
        }
    }

    /* Trim leading/trailing whitespace from path */
    {
        size_t len = strlen(path);
        while (len > 0 && (path[len - 1] == ' ' || path[len - 1] == '\t' || path[len - 1] == '\n' || path[len - 1] == '\r')) {
            path[--len] = '\0';
        }
        size_t start = 0;
        while (start < len && (path[start] == ' ' || path[start] == '\t' || path[start] == '\n' || path[start] == '\r')) {
            start++;
        }
        if (start > 0) {
            memmove(path, path + start, len - start + 1);
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
