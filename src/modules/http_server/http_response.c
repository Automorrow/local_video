#include "http_response.h"
#include "../../shared/log/log.h"
#include "../../include/platform.h"
#include <stdio.h>
#include <string.h>

void http_response_init(HttpResponse *resp)
{
    if (!resp) {
        return;
    }
    memset(resp, 0, sizeof(HttpResponse));
    resp->status_code = 200;
    resp->reason = "OK";
    resp->keep_alive = false;
}

const char *http_status_reason(int status_code)
{
    switch (status_code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default:  return "Unknown";
    }
}

lv_error_t http_response_send(int client_fd, const HttpResponse *resp, const char *body, size_t body_len)
{
    if (!resp || client_fd < 0) {
        return LV_ERROR_INVALID_ARG;
    }

    char header[1024];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: %s\r\n"
        "\r\n",
        resp->status_code,
        resp->reason,
        resp->content_type[0] ? resp->content_type : "text/plain",
        (long)body_len,
        resp->keep_alive ? "keep-alive" : "close"
    );

    if (header_len < 0 || (size_t)header_len >= sizeof(header)) {
        return LV_ERROR_IO;
    }

    ssize_t written = net_write(client_fd, header, (size_t)header_len);
    if (written != header_len) {
        return LV_ERROR_IO;
    }

    if (body && body_len > 0) {
        written = net_write(client_fd, body, body_len);
        if (written != (ssize_t)body_len) {
            return LV_ERROR_IO;
        }
    }

    return LV_OK;
}

lv_error_t http_response_send_error(int client_fd, int status_code, const char *message)
{
    if (client_fd < 0) {
        return LV_ERROR_INVALID_ARG;
    }

    HttpResponse resp;
    http_response_init(&resp);
    resp.status_code = status_code;
    resp.reason = http_status_reason(status_code);

    char body[512];
    int body_len = snprintf(body, sizeof(body),
        "<html><body><h1>%d %s</h1><p>%s</p></body></html>",
        status_code,
        resp.reason,
        message ? message : ""
    );

    return http_response_send(client_fd, &resp, body, (size_t)body_len);
}
