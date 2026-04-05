#include "api_handler.h"
#include "http_response.h"
#include "db_manager.h"
#include "video_scanner.h"
#include "module.h"
#include "log.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Response buffer size */
#define RESPONSE_SIZE (64 * 1024)

/* Read POST body from client socket */
static char *read_request_body(int client_fd)
{
    char *body = malloc(4096);
    if (!body) return NULL;
    
    ssize_t n = recv(client_fd, body, 4095, 0);
    if (n > 0) {
        body[n] = '\0';
    } else {
        free(body);
        body = NULL;
    }
    return body;
}

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} response_buffer_t;

static void buffer_init(response_buffer_t *buf)
{
    buf->capacity = 4096;
    buf->data = malloc(buf->capacity);
    buf->size = 0;
    if (buf->data) buf->data[0] = '\0';
}

static void buffer_free(response_buffer_t *buf)
{
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
}

static void buffer_append(response_buffer_t *buf, const char *str, size_t len)
{
    if (!buf->data) return;
    if (buf->size + len + 1 > buf->capacity) {
        buf->capacity = (buf->size + len + 4096) * 2;
        buf->data = realloc(buf->data, buf->capacity);
        if (!buf->data) return;
    }
    memcpy(buf->data + buf->size, str, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
}

static void buffer_append_str(response_buffer_t *buf, const char *str)
{
    buffer_append(buf, str, strlen(str));
}

/* Escape JSON string special characters */
static void buffer_append_json_str(response_buffer_t *buf, const char *str)
{
    if (!str) {
        buffer_append_str(buf, "null");
        return;
    }
    
    for (const char *p = str; *p; p++) {
        char c = *p;
        switch (c) {
            case '"': buffer_append_str(buf, "\\\""); break;
            case '\\': buffer_append_str(buf, "\\\\"); break;
            case '\b': buffer_append_str(buf, "\\b"); break;
            case '\f': buffer_append_str(buf, "\\f"); break;
            case '\n': buffer_append_str(buf, "\\n"); break;
            case '\r': buffer_append_str(buf, "\\r"); break;
            case '\t': buffer_append_str(buf, "\\t"); break;
            default:
                if ((unsigned char)c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", (unsigned char)c);
                    buffer_append_str(buf, esc);
                } else {
                    char tmp[2] = {c, '\0'};
                    buffer_append_str(buf, tmp);
                }
        }
    }
}

/* Video list callback */
static int video_list_callback(const VideoInfo *video, void *user_data)
{
    response_buffer_t *buf = (response_buffer_t *)user_data;
    char tmp[64];
    
    if (buf->size > 1) {
        buffer_append_str(buf, ",");
    }
    
    buffer_append_str(buf, "{\"id\":");
    snprintf(tmp, sizeof(tmp), "%ld", (long)video->id);
    buffer_append_str(buf, tmp);
    buffer_append_str(buf, ",\"title\":\"");
    buffer_append_json_str(buf, video->title);
    buffer_append_str(buf, "\",\"path\":\"");
    buffer_append_json_str(buf, video->path);
    buffer_append_str(buf, "\",\"category\":\"");
    buffer_append_json_str(buf, video->category);
    buffer_append_str(buf, "\",\"size\":");
    snprintf(tmp, sizeof(tmp), "%ld", (long)video->size);
    buffer_append_str(buf, tmp);
    buffer_append_str(buf, "}");
    return 0;
}

/* History callback */
static int history_callback(const HistoryInfo *history, void *user_data)
{
    response_buffer_t *buf = (response_buffer_t *)user_data;
    char tmp[64];
    
    if (buf->size > 1) {
        buffer_append_str(buf, ",");
    }
    
    buffer_append_str(buf, "{\"id\":");
    snprintf(tmp, sizeof(tmp), "%ld", (long)history->id);
    buffer_append_str(buf, tmp);
    buffer_append_str(buf, ",\"video_id\":");
    snprintf(tmp, sizeof(tmp), "%ld", (long)history->video_id);
    buffer_append_str(buf, tmp);
    buffer_append_str(buf, ",\"title\":\"");
    buffer_append_json_str(buf, history->video_title);
    buffer_append_str(buf, "\",\"path\":\"");
    buffer_append_json_str(buf, history->video_path);
    buffer_append_str(buf, "\",\"position\":");
    snprintf(tmp, sizeof(tmp), "%ld", (long)history->position);
    buffer_append_str(buf, tmp);
    buffer_append_str(buf, "}");
    return 0;
}

/* Favorite callback */
static int favorite_callback(const FavoriteInfo *favorite, void *user_data)
{
    response_buffer_t *buf = (response_buffer_t *)user_data;
    char tmp[64];
    
    if (buf->size > 1) {
        buffer_append_str(buf, ",");
    }
    
    buffer_append_str(buf, "{\"id\":");
    snprintf(tmp, sizeof(tmp), "%ld", (long)favorite->id);
    buffer_append_str(buf, tmp);
    buffer_append_str(buf, ",\"video_id\":");
    snprintf(tmp, sizeof(tmp), "%ld", (long)favorite->video_id);
    buffer_append_str(buf, tmp);
    buffer_append_str(buf, ",\"title\":\"");
    buffer_append_json_str(buf, favorite->video_title);
    buffer_append_str(buf, "\",\"path\":\"");
    buffer_append_json_str(buf, favorite->video_path);
    buffer_append_str(buf, "\"}");
    return 0;
}

/* Send JSON response */
static lv_error_t send_json_response(int client_fd, const char *json, int status_code)
{
    HttpResponse resp;
    http_response_init(&resp);
    resp.status_code = status_code;
    resp.reason = status_code == 200 ? "OK" : "Error";
    strncpy(resp.content_type, "application/json", HTTP_CONTENT_TYPE_MAX - 1);
    
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        status_code, resp.reason, (long)strlen(json));
    
    ssize_t w1 = write(client_fd, header, (size_t)header_len);
    ssize_t w2 = write(client_fd, json, strlen(json));
    (void)w1; (void)w2;
    return LV_OK;
}

/* Parse URL query parameters */
static void parse_query(const char *query, char *key, size_t key_size, char *value, size_t value_size)
{
    key[0] = '\0';
    value[0] = '\0';
    
    const char *eq = strchr(query, '=');
    if (!eq) return;
    
    size_t key_len = (size_t)(eq - query);
    if (key_len >= key_size) key_len = key_size - 1;
    memcpy(key, query, key_len);
    key[key_len] = '\0';
    
    const char *val = eq + 1;
    size_t value_len = strlen(val);
    if (value_len >= value_size) value_len = value_size - 1;
    memcpy(value, val, value_len);
    value[value_len] = '\0';
    
    /* URL decode */
    char *dst = value;
    const char *src = value;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            int hex1 = (src[1] >= 'a') ? (src[1] - 'a' + 10) : (src[1] >= 'A') ? (src[1] - 'A' + 10) : (src[1] - '0');
            int hex2 = (src[2] >= 'a') ? (src[2] - 'a' + 10) : (src[2] >= 'A') ? (src[2] - 'A' + 10) : (src[2] - '0');
            *dst++ = (char)((hex1 << 4) | hex2);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static int get_query_param(const char *query, const char *key, char *value, size_t value_size)
{
    if (!query || !key || !value) return -1;
    size_t key_len = strlen(key);
    const char *p = query;
    while (*p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            const char *val = p + key_len + 1;
            const char *end = strchr(val, '&');
            size_t len = end ? (size_t)(end - val) : strlen(val);
            if (len >= value_size) len = value_size - 1;
            memcpy(value, val, len);
            value[len] = '\0';
            return 0;
        }
        p = strchr(p, '&');
        if (!p) break;
        p++;
    }
    return -1;
}

/* API: GET /api/videos */
static lv_error_t api_get_videos(int client_fd, const char *query)
{
    response_buffer_t buf;
    buffer_init(&buf);

    buffer_append_str(&buf, "[");

    int limit = 0;
    int offset = 0;
    if (query) {
        char limit_str[16] = {0};
        char offset_str[16] = {0};
        get_query_param(query, "limit", limit_str, sizeof(limit_str));
        get_query_param(query, "offset", offset_str, sizeof(offset_str));
        if (limit_str[0]) limit = atoi(limit_str);
        if (offset_str[0]) offset = atoi(offset_str);
    }

    if (limit > 500) {
        free(buf.data);
        return send_json_response(client_fd, "{\"success\":false,\"error\":\"Limit exceeds maximum (500)\"}", 400);
    }
    if (limit < 0 || offset < 0) {
        free(buf.data);
        return send_json_response(client_fd, "{\"success\":false,\"error\":\"Invalid limit or offset\"}", 400);
    }

    char search_key[64] = {0};
    char search_value[256] = {0};
    if (query && !limit) {
        log_info("Parsing query: %s", query);
        parse_query(query, search_key, sizeof(search_key), search_value, sizeof(search_value));
        log_info("Search key: '%s', value: '%s'", search_key, search_value);
    } else if (query) {
        parse_query(query, search_key, sizeof(search_key), search_value, sizeof(search_value));
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

    buffer_append_str(&buf, "]");

    lv_error_t err = send_json_response(client_fd, buf.data, 200);
    buffer_free(&buf);
    return err;
}

/* API: GET /api/videos/random */
static lv_error_t api_get_random(int client_fd)
{
    response_buffer_t buf;
    buffer_init(&buf);
    
    buffer_append_str(&buf, "[");
    
    db_manager_video_get_random(1, video_list_callback, &buf);
    
    buffer_append_str(&buf, "]");
    
    lv_error_t err = send_json_response(client_fd, buf.data, 200);
    buffer_free(&buf);
    return err;
}

/* API: GET /api/history */
static lv_error_t api_get_history(int client_fd)
{
    response_buffer_t buf;
    buffer_init(&buf);
    
    buffer_append_str(&buf, "[");
    db_manager_history_get(history_callback, &buf);
    buffer_append_str(&buf, "]");
    
    lv_error_t err = send_json_response(client_fd, buf.data, 200);
    buffer_free(&buf);
    return err;
}

/* API: POST /api/history */
static lv_error_t api_add_history(int client_fd, const char *body)
{
    int64_t video_id = 0;
    int64_t position = 0;
    
    if (body) {
        sscanf(body, "{\"video_id\":%ld,\"position\":%ld}", &video_id, &position);
    }
    
    if (video_id > 0) {
        db_manager_history_add(video_id, position);
    }
    
    return send_json_response(client_fd, "{\"success\":true}", 200);
}

/* API: DELETE /api/history/:id */
static lv_error_t api_delete_history(int client_fd, const char *path)
{
    int64_t id = 0;
    if (path) {
        const char *id_start = strrchr(path, '/');
        if (id_start) {
            id_start++;
            id = strtoll(id_start, NULL, 10);
        }
    }

    if (id <= 0) {
        return send_json_response(client_fd, "{\"success\":false,\"error\":\"Invalid id\"}", 400);
    }

    lv_error_t err = db_manager_history_delete(id);

    if (err == LV_OK) {
        return send_json_response(client_fd, "{\"success\":true}", 200);
    }

    return send_json_response(client_fd, "{\"success\":false,\"error\":\"Failed to delete\"}", 500);
}

/* API: DELETE /api/history */
static lv_error_t api_clear_history(int client_fd)
{
    db_manager_history_clear();
    return send_json_response(client_fd, "{\"success\":true}", 200);
}

/* API: GET /api/favorites */
static lv_error_t api_get_favorites(int client_fd)
{
    response_buffer_t buf;
    buffer_init(&buf);
    
    buffer_append_str(&buf, "[");
    db_manager_favorites_list(favorite_callback, &buf);
    buffer_append_str(&buf, "]");
    
    lv_error_t err = send_json_response(client_fd, buf.data, 200);
    buffer_free(&buf);
    return err;
}

/* API: POST /api/favorites */
static lv_error_t api_add_favorite(int client_fd, const char *body)
{
    int64_t video_id = 0;
    if (body) {
        sscanf(body, "{\"video_id\":%ld}", &video_id);
    }
    
    if (video_id > 0) {
        db_manager_favorite_add(video_id);
    }
    
    return send_json_response(client_fd, "{\"success\":true}", 200);
}

/* API: DELETE /api/favorites/:id */
static lv_error_t api_remove_favorite(int client_fd, const char *path)
{
    int64_t video_id = 0;
    if (path) {
        sscanf(path, "/api/favorites/%ld", &video_id);
    }
    
    if (video_id > 0) {
        db_manager_favorite_remove(video_id);
    }
    
    return send_json_response(client_fd, "{\"success\":true}", 200);
}

/* API: GET /api/blacklist */
static int blacklist_list_callback(const BlacklistInfo *entry, void *user_data)
{
    response_buffer_t *buf = (response_buffer_t *)user_data;
    if (buf->size > 1) {
        buffer_append_str(buf, ",");
    }
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "{\"id\":%ld,\"path\":\"", (long)entry->id);
    buffer_append_str(buf, tmp);
    buffer_append_json_str(buf, entry->path);
    snprintf(tmp, sizeof(tmp), "\",\"created_at\":%ld}", (long)entry->created_at);
    buffer_append_str(buf, tmp);
    return 0;
}

static lv_error_t api_get_blacklist(int client_fd)
{
    response_buffer_t buf;
    buffer_init(&buf);
    
    buffer_append_str(&buf, "[");
    db_manager_blacklist_get_all(blacklist_list_callback, &buf);
    buffer_append_str(&buf, "]");
    
    lv_error_t err = send_json_response(client_fd, buf.data, 200);
    buffer_free(&buf);
    return err;
}

/* API: POST /api/blacklist */
static lv_error_t api_add_blacklist(int client_fd, const char *body)
{
    char path[512] = {0};
    
    if (body) {
        /* Parse path from JSON body */
        const char *path_start = strstr(body, "\"path\":\"");
        if (path_start) {
            path_start += 8;
            const char *path_end = strchr(path_start, '"');
            if (path_end) {
                size_t len = path_end - path_start;
                if (len < sizeof(path)) {
                    memcpy(path, path_start, len);
                    path[len] = '\0';
                }
            }
        }
    }
    
    /* Validate path */
    if (strlen(path) == 0) {
        return send_json_response(client_fd, "{\"success\":false,\"error\":\"Invalid path\"}", 400);
    }
    
    /* Check if directory exists */
    if (access(path, F_OK) != 0) {
        return send_json_response(client_fd, "{\"success\":false,\"error\":\"Directory does not exist\"}", 400);
    }
    
    /* Try to add to blacklist */
    int64_t new_id = 0;
    lv_error_t err = db_manager_blacklist_add(path, &new_id);

    if (err == LV_OK) {
        /* Mark videos in this directory as blacklisted (not delete) */
        db_manager_video_blacklist_by_path_prefix(path);

        char response[512];
        snprintf(response, sizeof(response), "{\"success\":true,\"data\":{\"id\":%ld}}", (long)new_id);
        return send_json_response(client_fd, response, 200);
    } else {
        /* Check if it's a UNIQUE constraint error */
        return send_json_response(client_fd, "{\"success\":false,\"error\":\"Directory already in blacklist\"}", 409);
    }
}

/* API: DELETE /api/blacklist/:id */
static lv_error_t api_remove_blacklist(int client_fd, const char *path)
{
    int64_t id = 0;
    if (path) {
        /* Extract ID from "/api/blacklist/:id" */
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
#ifndef _WIN32
            video_scanner_scan(blacklist_path);
#endif
        } else {
            db_manager_video_unblacklist_all();
        }

        return send_json_response(client_fd, "{\"success\":true,\"message\":\"Directory removed from blacklist and videos are being restored\"}", 200);
    }

    return send_json_response(client_fd, "{\"success\":false,\"error\":\"Blacklist entry not found\"}", 404);
}

/* API: GET /api/categories */
static int category_list_callback(const char *category, void *user_data)
{
    response_buffer_t *buf = (response_buffer_t *)user_data;
    if (buf->size > 1) {
        buffer_append_str(buf, ",");
    }
    buffer_append_str(buf, "{\"name\":\"");
    buffer_append_json_str(buf, category);
    buffer_append_str(buf, "\"}");
    return 0;
}

static lv_error_t api_get_categories(int client_fd)
{
    response_buffer_t buf;
    buffer_init(&buf);
    
    buffer_append_str(&buf, "[");
    db_manager_category_get_all(category_list_callback, &buf);
    buffer_append_str(&buf, "]");
    
    lv_error_t err = send_json_response(client_fd, buf.data, 200);
    buffer_free(&buf);
    return err;
}

/* Main API handler */
lv_error_t api_handler_handle(int client_fd, const HttpRequest *req)
{
    const char *path = req->path + 5; /* Skip "/api/" */
    
    if (strcmp(req->method, "GET") == 0) {
        if (strcmp(path, "videos") == 0) {
            return api_get_videos(client_fd, req->query[0] ? req->query : NULL);
        }
        if (strcmp(path, "videos/random") == 0 || strcmp(path, "random") == 0) {
            return api_get_random(client_fd);
        }
        if (strcmp(path, "history") == 0) {
            return api_get_history(client_fd);
        }
        if (strcmp(path, "favorites") == 0) {
            return api_get_favorites(client_fd);
        }
        if (strncmp(path, "favorites/", 10) == 0) {
            return api_get_favorites(client_fd); /* Same for now */
        }
        if (strcmp(path, "categories") == 0) {
            return api_get_categories(client_fd);
        }
        if (strcmp(path, "blacklist") == 0) {
            return api_get_blacklist(client_fd);
        }
    } else if (strcmp(req->method, "POST") == 0) {
        char *body = read_request_body(client_fd);
        if (strcmp(path, "history") == 0) {
            lv_error_t err = api_add_history(client_fd, body);
            free(body);
            return err;
        }
        if (strcmp(path, "favorites") == 0) {
            lv_error_t err = api_add_favorite(client_fd, body);
            free(body);
            return err;
        }
        if (strcmp(path, "blacklist") == 0) {
            lv_error_t err = api_add_blacklist(client_fd, body);
            free(body);
            return err;
        }
        free(body);
    } else if (strcmp(req->method, "DELETE") == 0) {
        if (strncmp(path, "history/", 8) == 0) {
            return api_delete_history(client_fd, req->path);
        }
        if (strcmp(path, "history") == 0) {
            return api_clear_history(client_fd);
        }
        if (strncmp(path, "favorites/", 10) == 0) {
            return api_remove_favorite(client_fd, req->path);
        }
        if (strncmp(path, "blacklist/", 10) == 0) {
            return api_remove_blacklist(client_fd, req->path);
        }
    }
    
    return send_json_response(client_fd, "{\"error\":\"Not found\"}", 404);
}

static void api_handler_init(void)
{
    log_info("API handler initialized");
}

static void api_handler_exit(void)
{
    log_info("API handler exited");
}

MODULE_INIT(api_handler_init, "api_handler");
MODULE_EXIT(api_handler_exit, "api_handler");
