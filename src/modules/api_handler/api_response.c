#include "api_handler_internal.h"
#include "../../shared/json/json.h"
#include "../../include/platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void api_buffer_init(response_buffer_t *buf)
{
    buf->capacity = 4096;
    buf->data = malloc(buf->capacity);
    buf->size = 0;
    if (buf->data) {
        buf->data[0] = '\0';
    }
}

void api_buffer_free(response_buffer_t *buf)
{
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
}

void api_buffer_append(response_buffer_t *buf, const char *str, size_t len)
{
    if (!buf->data) {
        return;
    }

    if (buf->size + len + 1 > buf->capacity) {
        size_t new_cap = (buf->size + len + 4096) * 2;
        char *tmp = realloc(buf->data, new_cap);
        if (!tmp) {
            return;  /* original buf->data still valid */
        }
        buf->data = tmp;
        buf->capacity = new_cap;
    }

    memcpy(buf->data + buf->size, str, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
}

void api_buffer_append_str(response_buffer_t *buf, const char *str)
{
    api_buffer_append(buf, str, strlen(str));
}

void api_buffer_append_json_str(response_buffer_t *buf, const char *str)
{
    char escaped[2048];

    if (!str) {
        api_buffer_append_str(buf, "null");
        return;
    }

    if (json_escape_string(str, escaped, sizeof(escaped)) != LV_OK) {
        return;
    }

    api_buffer_append_str(buf, escaped);
}

lv_error_t api_send_json_response(int client_fd, const char *json, int status_code)
{
    if (!json) json = "{}";

    HttpResponse resp;
    char header[512];
    int header_len;
    ssize_t w1;
    ssize_t w2;

    http_response_init(&resp);
    resp.status_code = status_code;
    resp.reason = status_code == 200 ? "OK" : "Error";
    strncpy(resp.content_type, "application/json", HTTP_CONTENT_TYPE_MAX - 1);

    header_len = snprintf(header, sizeof(header),
                          "HTTP/1.1 %d %s\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: close\r\n"
                          "Access-Control-Allow-Origin: http://localhost\r\n"
                          "\r\n",
                          status_code,
                          resp.reason,
                          strlen(json));

    w1 = net_write(client_fd, header, (size_t)header_len);
    w2 = net_write(client_fd, json, strlen(json));
    (void)w1;
    (void)w2;

    return LV_OK;
}
