#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include "local_video.h"
#include <stdint.h>

#define HTTP_METHOD_MAX 16
#define HTTP_PATH_MAX 512
#define HTTP_VERSION_MAX 16
#define HTTP_HOST_MAX 256
#define HTTP_CONN_MAX 16
#define HTTP_CONTENT_TYPE_MAX 64

typedef struct {
    char method[HTTP_METHOD_MAX];
    char path[HTTP_PATH_MAX];
    char query[HTTP_PATH_MAX];
    char version[HTTP_VERSION_MAX];
    char host[HTTP_HOST_MAX];
    char connection[HTTP_CONN_MAX];
    int64_t range_start;
    int64_t range_end;
    char if_modified_since[64];
} HttpRequest;

lv_error_t http_request_parse(int client_fd, HttpRequest *req);

#endif /* HTTP_REQUEST_H */
