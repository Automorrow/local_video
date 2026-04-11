#include "api_handler_internal.h"
#include "../../shared/module/module.h"
#include "../../shared/log/log.h"
#include <stdlib.h>
#include <string.h>

/* Main API handler */
lv_error_t api_handler_handle(int client_fd, const HttpRequest *req)
{
    if (strlen(req->path) < 5) {
        return api_send_json_response(client_fd, "{\"error\":\"Not found\"}", 404);
    }
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
        if (strcmp(path, "config") == 0) {
            return api_get_config(client_fd);
        }
        if (strcmp(path, "browse") == 0) {
            return api_browse_directories(client_fd, req->query);
        }
    } else if (strcmp(req->method, "POST") == 0) {
        char *body = api_read_request_body(client_fd, req->content_length);
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
        if (strcmp(path, "config") == 0) {
            lv_error_t err = api_update_config(client_fd, body);
            free(body);
            return err;
        }
        if (strcmp(path, "resolve-dir") == 0) {
            lv_error_t err = api_resolve_dir(client_fd, body);
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
    
    return api_send_json_response(client_fd, "{\"error\":\"Not found\"}", 404);
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
