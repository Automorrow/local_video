#include "http_server.h"
#include "http_request.h"
#include "http_response.h"
#include "module.h"
#include "../shared/log/log.h"
#include "config.h"
#include "../db_manager/db_manager.h"
#include "../../include/local_video.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

static volatile int server_running = 0;
static int server_socket = -1;
static pthread_t server_thread;
static uint16_t server_port = 8080;
static char web_root[512] = "./web/static";

/* Forward declaration for API handler */
extern lv_error_t api_handler_handle(int client_fd, const HttpRequest *req);

/* Get MIME type from file extension */
static const char *get_mime_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (ext) {
        ext++;
        if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return "text/html";
        if (strcmp(ext, "css") == 0) return "text/css";
        if (strcmp(ext, "js") == 0) return "application/javascript";
        if (strcmp(ext, "json") == 0) return "application/json";
        if (strcasecmp(ext, "mp4") == 0) return "video/mp4";
        if (strcasecmp(ext, "webm") == 0) return "video/webm";
        if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) return "image/jpeg";
        if (strcmp(ext, "png") == 0) return "image/png";
        if (strcmp(ext, "gif") == 0) return "image/gif";
        if (strcmp(ext, "ico") == 0) return "image/x-icon";
        if (strcmp(ext, "svg") == 0) return "image/svg+xml";
    }
    return "application/octet-stream";
}

/* Serve static file from web root */
static lv_error_t serve_static_file(int client_fd, const char *path)
{
    char full_path[1024];

    if (strstr(path, "..") != NULL) {
        return http_response_send_error(client_fd, 403, "Forbidden");
    }
    
    if (strcmp(path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "%s/index.html", web_root);
    } else {
        if (path[0] == '/') path++;
        snprintf(full_path, sizeof(full_path), "%s/%s", web_root, path);
    }

    struct stat lst;
    if (lstat(full_path, &lst) == 0 && S_ISLNK(lst.st_mode)) {
        return http_response_send_error(client_fd, 403, "Symlinks not allowed");
    }

    int fd = open(full_path, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            return http_response_send_error(client_fd, 404, "File not found");
        }
        return http_response_send_error(client_fd, 500, "Server error");
    }
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return http_response_send_error(client_fd, 500, "Server error");
    }
    
    HttpResponse resp;
    http_response_init(&resp);
    resp.status_code = 200;
    resp.reason = "OK";
    resp.content_length = st.st_size;
    strncpy(resp.content_type, get_mime_type(full_path), HTTP_CONTENT_TYPE_MAX - 1);
    
    char header[1024];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n",
        resp.content_type, (long)st.st_size);
    
    ssize_t written = write(client_fd, header, (size_t)header_len);
    if (written != header_len) {
        close(fd);
        return LV_ERROR_IO;
    }
    
    char buffer[8192];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read) {
            ssize_t n = write(client_fd, buffer + bytes_written, (size_t)(bytes_read - bytes_written));
            if (n < 0) {
                close(fd);
                return LV_ERROR_IO;
            }
            bytes_written += n;
        }
    }
    
    close(fd);
    return LV_OK;
}

/* Common image extensions to try */
static const char *image_extensions[] = {".jpg", ".jpeg", ".png", ".gif", ".webp", NULL};

/* Check if a file exists */
static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* Find preview image in directory (preview.jpg, preview.png, etc.) */
static void find_preview_image(const char *dir_path, char *preview_path, size_t path_size)
{
    preview_path[0] = '\0';
    if (!dir_path || !path_size) return;
    
    char base_path[1024];
    for (int i = 0; image_extensions[i]; i++) {
        snprintf(base_path, sizeof(base_path), "%s/preview%s", dir_path, image_extensions[i]);
        if (file_exists(base_path)) {
            size_t len = strlen(base_path);
            if (len >= path_size) len = path_size - 1;
            memcpy(preview_path, base_path, len);
            preview_path[len] = '\0';
            return;
        }
    }
}

/* Find thumbnail matching video base name (y.jpg, y.png, etc. for y.mp4) */
static void find_video_thumbnail(const char *video_path, char *thumb_path, size_t path_size)
{
    thumb_path[0] = '\0';
    if (!video_path || !path_size) return;
    
    const char *last_slash = strrchr(video_path, '/');
    const char *basename = last_slash ? last_slash + 1 : video_path;
    size_t dir_len = last_slash ? (size_t)(last_slash - video_path) : 0;

    // Get directory path
    char dir[1024] = {0};
    if (dir_len > 0) {
        if (dir_len >= sizeof(dir)) dir_len = sizeof(dir) - 1;
        strncpy(dir, video_path, dir_len);
        dir[dir_len] = '\0';
    } else {
        strcpy(dir, ".");
    }

    // Get basename without extension (limit to 512 chars to avoid overflow)
    const char *ext = strrchr(basename, '.');
    size_t name_len = ext ? (size_t)(ext - basename) : strlen(basename);
    if (name_len > 512) name_len = 512;
    
    char base_name[513];
    if (name_len >= sizeof(base_name)) name_len = sizeof(base_name) - 1;
    memcpy(base_name, basename, name_len);
    base_name[name_len] = '\0';

    // Try all image extensions
    char full_path[2048];
    for (int i = 0; image_extensions[i]; i++) {
        snprintf(full_path, sizeof(full_path), "%s/%s%s", dir, base_name, image_extensions[i]);
        if (file_exists(full_path)) {
            size_t len = strlen(full_path);
            if (len >= path_size) len = path_size - 1;
            memcpy(thumb_path, full_path, len);
            thumb_path[len] = '\0';
            return;
        }
    }
}

static lv_error_t serve_thumbnail(int client_fd, int64_t video_id)
{
    VideoInfo video;
    if (db_manager_video_get_by_id(video_id, &video) != LV_OK) {
        http_response_send_error(client_fd, 404, "Thumbnail not found");
        return LV_ERROR_IO;
    }

    struct stat st;
    char preview_path[1024] = {0};
    char thumb_path[1024] = {0};
    char imgs_path[2048] = {0};

    /* Step 1: Get video directory and basename */
    const char *last_slash = strrchr(video.path, '/');
    const char *basename = last_slash ? last_slash + 1 : video.path;
    
    size_t dir_len = last_slash ? (size_t)(last_slash - video.path) : 0;
    char video_dir[1024] = {0};
    if (dir_len > 0) {
        if (dir_len >= sizeof(video_dir)) dir_len = sizeof(video_dir) - 1;
        memcpy(video_dir, video.path, dir_len);
        video_dir[dir_len] = '\0';
    } else {
        strcpy(video_dir, ".");
    }

    /* Step 2: Check for imgs/ directory (replace video/ with imgs/) */
    char parent_dir[1024] = {0};
    char base_name_noext[256] = {0};
    
    const char *video_slash = strstr(video_dir, "/video");
    if (video_slash) {
        // Build parent directory path (before /video)
        size_t parent_len = (size_t)(video_slash - video_dir);
        if (parent_len >= sizeof(parent_dir)) parent_len = sizeof(parent_dir) - 1;
        memcpy(parent_dir, video_dir, parent_len);
        parent_dir[parent_len] = '\0';
        
        // Try imgs/ directory first
        char imgs_dir[2048];
        snprintf(imgs_dir, sizeof(imgs_dir), "%s/imgs", parent_dir);
        
        // Get basename without extension and remove "-test" suffix
        const char *ext = strrchr(basename, '.');
        size_t name_len = ext ? (size_t)(ext - basename) : strlen(basename);
        if (name_len >= sizeof(base_name_noext)) name_len = sizeof(base_name_noext) - 1;
        strncpy(base_name_noext, basename, name_len);
        base_name_noext[name_len] = '\0';
        
        // Remove "-test" suffix if present
        char *test_suffix = strstr(base_name_noext, "-test");
        if (test_suffix) *test_suffix = '\0';
        
        // Try all image extensions in imgs/ directory
        for (int i = 0; image_extensions[i]; i++) {
            snprintf(imgs_path, sizeof(imgs_path), "%s/%s%s", imgs_dir, base_name_noext, image_extensions[i]);
            if (file_exists(imgs_path)) {
                break;
            }
            imgs_path[0] = '\0';
        }
    }
    
    /* Step 3: If found imgs/ thumbnail, serve it */
    if (imgs_path[0] != '\0' && stat(imgs_path, &st) == 0) {
        int fd = open(imgs_path, O_RDONLY);
        if (fd < 0) {
            http_response_send_error(client_fd, 500, "Server error");
            return LV_ERROR_IO;
        }
        char header[256];
        int header_len = snprintf(header, sizeof(header),
                                  "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", get_mime_type(imgs_path), (long)st.st_size);
        if (write(client_fd, header, (size_t)header_len) != header_len) {
            close(fd);
            return LV_ERROR_IO;
        }
        char buffer[8192];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            if (write(client_fd, buffer, (size_t)bytes_read) < 0) break;
        }
        close(fd);
        return LV_OK;
    }

    /* Step 3.5: Try preview.* in the same directory as the video */
    find_preview_image(video_dir, preview_path, sizeof(preview_path));
    if (preview_path[0] != '\0' && stat(preview_path, &st) == 0) {
        int fd = open(preview_path, O_RDONLY);
        if (fd < 0) {
            http_response_send_error(client_fd, 500, "Server error");
            return LV_ERROR_IO;
        }
        char header[256];
        int header_len = snprintf(header, sizeof(header),
                                  "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", get_mime_type(preview_path), (long)st.st_size);
        if (write(client_fd, header, (size_t)header_len) != header_len) {
            close(fd);
            return LV_ERROR_IO;
        }
        char buffer[8192];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            if (write(client_fd, buffer, (size_t)bytes_read) < 0) break;
        }
        close(fd);
        return LV_OK;
    }

    /* Step 4: Try preview.* in parent directory (if we have one) */
    if (parent_dir[0] != '\0') {
        find_preview_image(parent_dir, preview_path, sizeof(preview_path));
        if (preview_path[0] != '\0' && stat(preview_path, &st) == 0) {
            int fd = open(preview_path, O_RDONLY);
            if (fd < 0) {
                http_response_send_error(client_fd, 500, "Server error");
                return LV_ERROR_IO;
            }
            char header[256];
            int header_len = snprintf(header, sizeof(header),
                                      "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", get_mime_type(preview_path), (long)st.st_size);
            if (write(client_fd, header, (size_t)header_len) != header_len) {
                close(fd);
                return LV_ERROR_IO;
            }
            char buffer[8192];
            ssize_t bytes_read;
            while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
                if (write(client_fd, buffer, (size_t)bytes_read) < 0) break;
            }
            close(fd);
            return LV_OK;
        }
    }

    /* Step 5: Fallback: use <video-basename> image (any format) next to the video file */
    find_video_thumbnail(video.path, thumb_path, sizeof(thumb_path));
    if (thumb_path[0] != '\0' && stat(thumb_path, &st) == 0) {
        int fd = open(thumb_path, O_RDONLY);
        if (fd < 0) {
            http_response_send_error(client_fd, 500, "Server error");
            return LV_ERROR_IO;
        }
        char header[256];
        int header_len = snprintf(header, sizeof(header),
                                  "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", get_mime_type(thumb_path), (long)st.st_size);
        if (write(client_fd, header, (size_t)header_len) != header_len) {
            close(fd);
            return LV_ERROR_IO;
        }
        char buffer[8192];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            if (write(client_fd, buffer, (size_t)bytes_read) < 0) break;
        }
        close(fd);
        return LV_OK;
    }

    // Thumbnail does not exist
    http_response_send_error(client_fd, 404, "Thumbnail not found");
    return LV_ERROR_IO;
}

/* Serve video file with streaming support */
static lv_error_t serve_video_stream(int client_fd, const HttpRequest *req, const char *video_path)
{
    log_info("Serving video: %s", video_path);

    struct stat lst;
    if (lstat(video_path, &lst) == 0 && S_ISLNK(lst.st_mode)) {
        return http_response_send_error(client_fd, 403, "Symlinks not allowed");
    }

    int fd = open(video_path, O_RDONLY);
    if (fd < 0) {
        log_error("Cannot open video file %s: %s", video_path, strerror(errno));
        if (errno == ENOENT) return http_response_send_error(client_fd, 404, "Video not found");
        return http_response_send_error(client_fd, 500, "Server error");
    }
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        log_error("Cannot stat video file: %s", strerror(errno));
        close(fd);
        return http_response_send_error(client_fd, 500, "Server error");
    }
    
    log_info("Video file size: %ld bytes", (long)st.st_size);
    
    int64_t file_size = (int64_t)st.st_size;
    int64_t range_start = 0;
    int64_t range_end = file_size - 1;
    bool is_range = (req->range_start >= 0 || req->range_end >= 0);
    
    if (is_range) {
        if (req->range_start >= 0) range_start = req->range_start;
        if (req->range_end >= 0) range_end = req->range_end;
        if (range_end >= file_size) range_end = file_size - 1;
        if (range_start > range_end) range_start = 0;
    }
    
    if (is_range) {
        int64_t content_length = range_end - range_start + 1;
        char header[1024];
        int header_len = snprintf(header, sizeof(header),
            "HTTP/1.1 206 Partial Content\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Content-Range: bytes %ld-%ld/%ld\r\n"
            "Connection: close\r\n"
            "Accept-Ranges: bytes\r\n"
            "\r\n",
            get_mime_type(video_path), (long)content_length,
            (long)range_start, (long)range_end, (long)file_size);
        
        ssize_t written = write(client_fd, header, (size_t)header_len);
        if (written != header_len) { close(fd); return LV_ERROR_IO; }
        
        lseek(fd, range_start, SEEK_SET);
        int64_t remaining = content_length;
        char buffer[65536];
        while (remaining > 0) {
            size_t to_read = remaining > (int64_t)sizeof(buffer) ? sizeof(buffer) : (size_t)remaining;
            ssize_t bytes_read = read(fd, buffer, to_read);
            if (bytes_read <= 0) break;
            ssize_t bytes_written = write(client_fd, buffer, (size_t)bytes_read);
            if (bytes_written < 0) {
                if (errno == EPIPE || errno == ECONNRESET) {
                    log_debug("Client disconnected during video stream: %s", strerror(errno));
                } else {
                    log_error("Write error: %s", strerror(errno));
                }
                break;
            }
            remaining -= bytes_written;
        }
    } else {
        char header[1024];
        int header_len = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "Accept-Ranges: bytes\r\n"
            "\r\n",
            get_mime_type(video_path), (long)file_size);
        
        ssize_t written = write(client_fd, header, (size_t)header_len);
        if (written != header_len) { close(fd); return LV_ERROR_IO; }
        
        char buffer[65536];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            ssize_t bytes_written = write(client_fd, buffer, (size_t)bytes_read);
            if (bytes_written < 0) {
                if (errno == EPIPE || errno == ECONNRESET) {
                    log_debug("Client disconnected during video stream: %s", strerror(errno));
                } else {
                    log_error("Write error: %s", strerror(errno));
                }
                break;
            }
        }
    }
    
    close(fd);
    log_info("Video served successfully");
    return LV_OK;
}

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
            serve_thumbnail(client_fd, video_id);
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
                err = serve_video_stream(client_fd, &req, video.path);
                if (err != LV_OK) http_response_send_error(client_fd, 500, "Video error");
            }
        } else {
            http_response_send_error(client_fd, 400, "Invalid path");
        }
    } else {
        err = serve_static_file(client_fd, req.path);
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
