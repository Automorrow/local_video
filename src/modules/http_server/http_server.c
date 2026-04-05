#include "http_server.h"
#include "http_request.h"
#include "http_response.h"
#include "http_server_internal.h"
#include "../../shared/module/module.h"
#include "../../shared/log/log.h"
#include "../config/config.h"
#include "../db_manager/db_manager.h"
#include "../../include/local_video.h"
#include "../../include/platform.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static volatile int server_running = 0;
static int server_socket = -1;
static pthread_t server_thread;
static uint16_t server_port = 8080;
static char web_root[512] = "./web/static";

/* Forward declaration for API handler */
extern lv_error_t api_handler_handle(int client_fd, const HttpRequest *req);


static void *connection_handler(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    HttpRequest req;
    lv_error_t err = http_request_parse(client_fd, &req);

    if (err != LV_OK) {
        http_response_send_error(client_fd, 400, "Bad request");
        close(client_fd);
        return NULL;
    }

    log_info("HTTP: %s %s %s", req.method, req.path, req.version);

    /* For API endpoints, allow GET, POST, DELETE */
    if (strncmp(req.path, "/api/", 5) == 0) {
        if (strcmp(req.method, "GET") != 0 && strcmp(req.method, "POST") != 0 && strcmp(req.method, "DELETE") != 0) {
            http_response_send_error(client_fd, 405, "Method not allowed");
            close(client_fd);
            return NULL;
        }
    } else {
        /* For static files and video streaming, only allow GET */
        if (strcmp(req.method, "GET") != 0) {
            http_response_send_error(client_fd, 405, "Method not allowed");
            close(client_fd);
            return NULL;
        }
    }
    if (strncmp(req.path, "/api/", 5) == 0) {
        err = api_handler_handle(client_fd, &req);
        if (err != LV_OK) http_response_send_error(client_fd, 500, "API error");
    } else if (strncmp(req.path, "/thumbnail/", 11) == 0) {
        char video_id_str[64];
        if (sscanf(req.path, "/thumbnail/%63s", video_id_str) == 1) {
            int64_t video_id = strtoll(video_id_str, NULL, 10);
            http_server_serve_thumbnail(client_fd, video_id);
        } else {
            http_response_send_error(client_fd, 400, "Invalid thumbnail path");
        }
    } else if (strncmp(req.path, "/video/", 7) == 0) {
        char video_id_str[64];
        if (sscanf(req.path, "/video/%63s", video_id_str) == 1) {
            int64_t video_id = strtoll(video_id_str, NULL, 10);
            log_info("Fetching video info for ID: %ld", (long)video_id);
            VideoInfo video;
            memset(&video, 0, sizeof(video));
            err = db_manager_video_get_by_id(video_id, &video);
            if (err != LV_OK) {
                log_error("Video not found for ID: %ld", (long)video_id);
                http_response_send_error(client_fd, 404, "Video not found");
            } else {
                log_info("Video path from DB: %s", video.path);
                err = http_server_serve_video_stream(client_fd, &req, video.path);
                if (err != LV_OK) http_response_send_error(client_fd, 500, "Video error");
            }
        } else {
            http_response_send_error(client_fd, 400, "Invalid path");
        }
    } else {
        err = http_server_serve_static_file(client_fd, req.path, web_root);
        if (err != LV_OK) http_response_send_error(client_fd, 500, "Static file error");
    }

    close(client_fd);
    return NULL;
}

static void *server_loop(void *arg)
{
    (void)arg;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        log_error("Failed to create socket: %s", strerror(errno));
        return NULL;
    }

    int opt = 1;
#ifdef _WIN32
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) < 0) {
#else
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
#endif
        log_warning("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server_port);

    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("Failed to bind to port %d: %s", server_port, strerror(errno));
        close(server_socket);
        server_socket = -1;
        return NULL;
    }

    if (listen(server_socket, 64) < 0) {
        log_error("Failed to listen on port %d: %s", server_port, strerror(errno));
        close(server_socket);
        server_socket = -1;
        return NULL;
    }

    log_info("HTTP server listening on port %d", server_port);

    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (server_running) {
                log_error("Failed to accept connection: %s", strerror(errno));
            }
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        log_debug("Accepted connection from %s:%d", client_ip, ntohs(client_addr.sin_port));

        int *client_fd_ptr = malloc(sizeof(int));
        if (!client_fd_ptr) {
            log_error("Failed to allocate memory for client fd");
            close(client_fd);
            continue;
        }
        *client_fd_ptr = client_fd;

        pthread_t thread;
        if (pthread_create(&thread, NULL, connection_handler, client_fd_ptr) != 0) {
            log_error("Failed to create thread: %s", strerror(errno));
            free(client_fd_ptr);
            close(client_fd);
            continue;
        }

        pthread_detach(thread);
    }

    return NULL;
}

lv_error_t http_server_init(uint16_t port, const char *web_root_path)
{
    if (port == 0) {
        return LV_ERROR_INVALID_ARG;
    }

    server_port = port;
    if (web_root_path) {
        strncpy(web_root, web_root_path, sizeof(web_root) - 1);
        web_root[sizeof(web_root) - 1] = '\0';
    }

    log_info("HTTP server initialized on port %d, web_root: %s", server_port, web_root);
    return LV_OK;
}

lv_error_t http_server_start(void)
{
    if (server_running) {
        return LV_OK;
    }

    server_running = 1;

    if (pthread_create(&server_thread, NULL, server_loop, NULL) != 0) {
        log_error("Failed to create server thread: %s", strerror(errno));
        server_running = 0;
        return LV_ERROR_UNKNOWN;
    }

    log_info("HTTP server started");
    return LV_OK;
}

lv_error_t http_server_stop(void)
{
    if (!server_running) {
        return LV_OK;
    }

    server_running = 0;

    if (server_socket >= 0) {
        shutdown(server_socket, SHUT_RDWR);
    }

    pthread_join(server_thread, NULL);

    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }

    log_info("HTTP server stopped");
    return LV_OK;
}

lv_error_t http_server_close(void)
{
    return http_server_stop();
}

static void http_server_run(void)
{
    http_server_start();
}

static void http_server_module_init(void)
{
    const lv_config_t *cfg = config_get();
    http_server_init(cfg->http_port, cfg->web_root);
}

static void http_server_module_exit(void)
{
    http_server_stop();
}

MODULE_INIT(http_server_module_init, "http_server");
MODULE_EXIT(http_server_module_exit, "http_server");
MODULE_RUN(http_server_run, "http_server");
