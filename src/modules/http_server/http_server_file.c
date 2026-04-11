#include "http_server_internal.h"
#include "http_response.h"
#include "../../include/platform.h"
#include "../../shared/log/log.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

const char *http_server_get_mime_type(const char *path)
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
        if (strcmp(ext, "webp") == 0) return "image/webp";
    }
    return "application/octet-stream";
}

static lv_error_t write_all(int fd, const char *buffer, size_t len)
{
    size_t written_total = 0;

    while (written_total < len) {
        ssize_t written = net_write(fd, buffer + written_total, len - written_total);
        if (written < 0) {
            log_error("net_write failed: errno=%d", SOCKET_ERRNO);
            return LV_ERROR_IO;
        }
        written_total += (size_t)written;
    }

    return LV_OK;
}

lv_error_t http_server_send_file_response(int client_fd,
                                          const char *file_path,
                                          int64_t range_start,
                                          int64_t range_end,
                                          int enable_range)
{
    FILE *fp;
    int64_t file_size;
    int64_t actual_start;
    int64_t actual_end;
    char header[1024];
    int header_len;
    char buffer[65536];

    if (!file_path || client_fd < 0) {
        return LV_ERROR_INVALID_ARG;
    }

    fp = fopen(file_path, "rb");
    if (!fp) {
        log_error("Failed to open file '%s': errno=%d", file_path, errno);
        if (errno == ENOENT) {
            return http_response_send_error(client_fd, 404, "File not found");
        }
        return http_response_send_error(client_fd, 500, "Server error");
    }

    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) {
        log_error("Failed to seek file '%s': errno=%d", file_path, errno);
        fclose(fp);
        return http_response_send_error(client_fd, 500, "Server error");
    }
    file_size = (int64_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    log_info("Serving file '%s' (size=%ld bytes)", file_path, (long)file_size);

    actual_start = 0;
    actual_end = file_size > 0 ? file_size - 1 : 0;

    if (enable_range && file_size > 0) {
        if (range_start >= 0) actual_start = range_start;
        if (range_end >= 0) actual_end = range_end;
        if (actual_end >= file_size) actual_end = file_size - 1;
        if (actual_start > actual_end) actual_start = 0;
    }

    if (enable_range && file_size > 0) {
        int64_t content_length = actual_end - actual_start + 1;

        header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 206 Partial Content\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %lld\r\n"
                              "Content-Range: bytes %lld-%lld/%lld\r\n"
                              "Connection: close\r\n"
                              "Accept-Ranges: bytes\r\n"
                              "\r\n",
                              http_server_get_mime_type(file_path),
                              (long long)content_length,
                              (long long)actual_start,
                              (long long)actual_end,
                              (long long)file_size);
        if (header_len < 0 || (size_t)header_len >= sizeof(header)) {
            fclose(fp);
            return LV_ERROR_IO;
        }

        if (write_all(client_fd, header, (size_t)header_len) != LV_OK) {
            fclose(fp);
            return LV_ERROR_IO;
        }

        fseek(fp, actual_start, SEEK_SET);

        while (content_length > 0) {
            size_t to_read = content_length > (int64_t)sizeof(buffer) ?
                             sizeof(buffer) : (size_t)content_length;
            size_t bytes_read = fread(buffer, 1, to_read, fp);
            if (bytes_read == 0) {
                break;
            }
            if (write_all(client_fd, buffer, bytes_read) != LV_OK) {
                fclose(fp);
                return LV_ERROR_IO;
            }
            content_length -= (int64_t)bytes_read;
        }
    } else {
        header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %lld\r\n"
                              "Connection: close\r\n"
                              "%s"
                              "\r\n",
                              http_server_get_mime_type(file_path),
                              (long long)file_size,
                              enable_range ? "Accept-Ranges: bytes\r\n" : "");
        if (header_len < 0 || (size_t)header_len >= sizeof(header)) {
            fclose(fp);
            return LV_ERROR_IO;
        }

        if (write_all(client_fd, header, (size_t)header_len) != LV_OK) {
            fclose(fp);
            return LV_ERROR_IO;
        }

        while (1) {
            size_t bytes_read = fread(buffer, 1, sizeof(buffer), fp);
            if (bytes_read == 0) {
                break;
            }
            if (write_all(client_fd, buffer, bytes_read) != LV_OK) {
                fclose(fp);
                return LV_ERROR_IO;
            }
        }
    }

    fclose(fp);
    return LV_OK;
}

lv_error_t http_server_serve_static_file(int client_fd,
                                         const char *path,
                                         const char *web_root)
{
    char full_path[1024];
    const char *relative_path = path;

    if (!path || !web_root) {
        return LV_ERROR_INVALID_ARG;
    }

    if (strstr(path, "..") != NULL) {
        return http_response_send_error(client_fd, 403, "Forbidden");
    }

    if (strcmp(path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "%s/index.html", web_root);
    } else {
        if (relative_path[0] == '/') {
            relative_path++;
        }
        snprintf(full_path, sizeof(full_path), "%s/%s", web_root, relative_path);
    }

    return http_server_send_file_response(client_fd, full_path, 0, 0, 0);
}
