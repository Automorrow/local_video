#ifndef API_HANDLER_INTERNAL_H
#define API_HANDLER_INTERNAL_H

#include "api_handler.h"
#include "../db_manager/db_manager.h"
#include "../http_server/http_response.h"
#include <stddef.h>

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} response_buffer_t;

char *api_read_request_body(int client_fd);

void api_buffer_init(response_buffer_t *buf);
void api_buffer_free(response_buffer_t *buf);
void api_buffer_append(response_buffer_t *buf, const char *str, size_t len);
void api_buffer_append_str(response_buffer_t *buf, const char *str);
void api_buffer_append_json_str(response_buffer_t *buf, const char *str);

lv_error_t api_send_json_response(int client_fd, const char *json, int status_code);

void api_parse_query(const char *query,
                     char *key,
                     size_t key_size,
                     char *value,
                     size_t value_size);
int api_get_query_param(const char *query,
                        const char *key,
                        char *value,
                        size_t value_size);

lv_error_t api_get_videos(int client_fd, const char *query);
lv_error_t api_get_random(int client_fd);
lv_error_t api_get_history(int client_fd);
lv_error_t api_add_history(int client_fd, const char *body);
lv_error_t api_delete_history(int client_fd, const char *path);
lv_error_t api_clear_history(int client_fd);
lv_error_t api_get_favorites(int client_fd);
lv_error_t api_add_favorite(int client_fd, const char *body);
lv_error_t api_remove_favorite(int client_fd, const char *path);
lv_error_t api_get_blacklist(int client_fd);
lv_error_t api_add_blacklist(int client_fd, const char *body);
lv_error_t api_remove_blacklist(int client_fd, const char *path);
lv_error_t api_get_categories(int client_fd);
lv_error_t api_get_config(int client_fd);
lv_error_t api_update_config(int client_fd, const char *body);
lv_error_t api_browse_directories(int client_fd, const char *query);
lv_error_t api_resolve_dir(int client_fd, const char *body);

#endif
