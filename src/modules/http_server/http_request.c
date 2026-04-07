#include "http_request.h"
#include "../../shared/log/log.h"
#include "../../include/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_line(int fd, char *buffer, size_t max_len)
{
    size_t pos = 0;
    char c;

    while (pos < max_len - 1) {
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) {
            return -1;
        }
        if (c == '\r') {
            n = read(fd, &c, 1);
            if (n > 0 && c == '\n') {
                buffer[pos] = '\0';
                return 0;
            }
            return -1;
        }
        if (c == '\n') {
            buffer[pos] = '\0';
            return 0;
        }
        buffer[pos++] = c;
    }

    return -1;
}

static int parse_request_line(const char *line, HttpRequest *req)
{
    char method[HTTP_METHOD_MAX];
    char path_with_query[HTTP_PATH_MAX];
    char version[HTTP_VERSION_MAX];

    int n = sscanf(line, "%15s %511s %15s", method, path_with_query, version);
    if (n != 3) {
        return -1;
    }

    strncpy(req->method, method, HTTP_METHOD_MAX - 1);
    req->method[HTTP_METHOD_MAX - 1] = '\0';

    /* Separate path and query */
    char *question_mark = strchr(path_with_query, '?');
    if (question_mark) {
        /* Copy path part */
        size_t path_len = (size_t)(question_mark - path_with_query);
        if (path_len >= HTTP_PATH_MAX) path_len = HTTP_PATH_MAX - 1;
        memcpy(req->path, path_with_query, path_len);
        req->path[path_len] = '\0';
        
        /* Copy query part */
        strncpy(req->query, question_mark + 1, HTTP_PATH_MAX);
        req->query[HTTP_PATH_MAX - 1] = '\0';
    } else {
        /* No query string */
        strncpy(req->path, path_with_query, HTTP_PATH_MAX);
        req->path[HTTP_PATH_MAX - 1] = '\0';
        req->query[0] = '\0';
    }

    strncpy(req->version, version, HTTP_VERSION_MAX - 1);
    req->version[HTTP_VERSION_MAX - 1] = '\0';

    return 0;
}

static int parse_range_header(const char *value, int64_t *start, int64_t *end)
{
    if (strncmp(value, "bytes=", 6) != 0) {
        return -1;
    }

    char *dash = strchr(value + 6, '-');
    if (!dash) {
        return -1;
    }

    if (dash == value + 6) {
        *start = -1;
    } else {
        *start = (int64_t)strtoll(value + 6, NULL, 10);
    }

    if (dash[1] == '\0') {
        *end = -1;
    } else {
        *end = (int64_t)strtoll(dash + 1, NULL, 10);
    }

    return 0;
}

static int parse_header(const char *line, HttpRequest *req)
{
    char header_name[128];
    char header_value[256];

    const char *colon = strchr(line, ':');
    if (!colon) {
        return -1;
    }

    size_t name_len = colon - line;
    if (name_len >= sizeof(header_name)) {
        return -1;
    }

    strncpy(header_name, line, name_len);
    header_name[name_len] = '\0';

    const char *value_start = colon + 1;
    while (*value_start == ' ') {
        value_start++;
    }

    strncpy(header_value, value_start, sizeof(header_value) - 1);
    header_value[sizeof(header_value) - 1] = '\0';

    if (strcasecmp(header_name, "Host") == 0) {
        strncpy(req->host, header_value, HTTP_HOST_MAX - 1);
        req->host[HTTP_HOST_MAX - 1] = '\0';
    } else if (strcasecmp(header_name, "Connection") == 0) {
        strncpy(req->connection, header_value, HTTP_CONN_MAX - 1);
        req->connection[HTTP_CONN_MAX - 1] = '\0';
    } else if (strcasecmp(header_name, "Range") == 0) {
        parse_range_header(header_value, &req->range_start, &req->range_end);
    } else if (strcasecmp(header_name, "If-Modified-Since") == 0) {
        strncpy(req->if_modified_since, header_value, sizeof(req->if_modified_since) - 1);
        req->if_modified_since[sizeof(req->if_modified_since) - 1] = '\0';
    }

    return 0;
}

lv_error_t http_request_parse(int client_fd, HttpRequest *req)
{
    if (!req) {
        return LV_ERROR_INVALID_ARG;
    }

    memset(req, 0, sizeof(HttpRequest));
    req->range_start = -1;
    req->range_end = -1;

    char line[1024];

    if (read_line(client_fd, line, sizeof(line)) != 0) {
        return LV_ERROR_IO;
    }

    if (parse_request_line(line, req) != 0) {
        return LV_ERROR_INVALID_ARG;
    }

    while (read_line(client_fd, line, sizeof(line)) == 0) {
        if (line[0] == '\0') {
            break;
        }
        parse_header(line, req);
    }

    return LV_OK;
}
