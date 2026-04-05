#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "../../include/local_video.h"
#include <stdbool.h>

#define HTTP_CONTENT_TYPE_MAX 64

typedef struct {
    int status_code;
    const char *reason;
    char content_type[HTTP_CONTENT_TYPE_MAX];
    int64_t content_length;
    bool keep_alive;
} HttpResponse;

void http_response_init(HttpResponse *resp);
lv_error_t http_response_send(int client_fd, const HttpResponse *resp, const char *body, size_t body_len);
lv_error_t http_response_send_error(int client_fd, int status_code, const char *message);
const char *http_status_reason(int status_code);

#endif /* HTTP_RESPONSE_H */
