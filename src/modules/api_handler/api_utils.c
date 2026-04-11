#include "api_handler_internal.h"
#include "../../include/platform.h"
#include <stdlib.h>
#include <string.h>

char *api_read_request_body(int client_fd, int64_t content_length)
{
    if (content_length <= 0) {
        return NULL;
    }

#define MAX_BODY_SIZE 65536
    int64_t to_read = content_length;
    if (to_read > MAX_BODY_SIZE) {
        to_read = MAX_BODY_SIZE;
    }

    char *body = malloc((size_t)to_read + 1);
    if (!body) {
        return NULL;
    }

    int64_t received = 0;
    while (received < to_read) {
        ssize_t n = recv(client_fd, body + received, (size_t)(to_read - received), 0);
        if (n <= 0) {
            free(body);
            return NULL;
        }
        received += n;
    }
    body[received] = '\0';
    return body;
#undef MAX_BODY_SIZE
}

void api_parse_query(const char *query,
                     char *key,
                     size_t key_size,
                     char *value,
                     size_t value_size)
{
    const char *eq;
    const char *val;
    size_t key_len;
    size_t value_len;
    char *dst;
    const char *src;

    key[0] = '\0';
    value[0] = '\0';

    eq = strchr(query, '=');
    if (!eq) {
        return;
    }

    key_len = (size_t)(eq - query);
    if (key_len >= key_size) {
        key_len = key_size - 1;
    }
    memcpy(key, query, key_len);
    key[key_len] = '\0';

    val = eq + 1;
    const char *amp = strchr(val, '&');
    if (amp) {
        value_len = (size_t)(amp - val);
    } else {
        value_len = strlen(val);
    }
    if (value_len >= value_size) {
        value_len = value_size - 1;
    }
    memcpy(value, val, value_len);
    value[value_len] = '\0';

    dst = value;
    src = value;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            int hex1 = (src[1] >= 'a') ? (src[1] - 'a' + 10) : (src[1] >= 'A') ? (src[1] - 'A' + 10) : (src[1] - '0');
            int hex2 = (src[2] >= 'a') ? (src[2] - 'a' + 10) : (src[2] >= 'A') ? (src[2] - 'A' + 10) : (src[2] - '0');
            *dst++ = (char)((hex1 << 4) | hex2);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

int api_get_query_param(const char *query,
                        const char *key,
                        char *value,
                        size_t value_size)
{
    size_t key_len;
    const char *p;

    if (!query || !key || !value) {
        return -1;
    }

    key_len = strlen(key);
    p = query;
    while (*p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            const char *val = p + key_len + 1;
            const char *end = strchr(val, '&');
            size_t len = end ? (size_t)(end - val) : strlen(val);
            if (len >= value_size) {
                len = value_size - 1;
            }
            memcpy(value, val, len);
            value[len] = '\0';
            return 0;
        }
        p = strchr(p, '&');
        if (!p) {
            break;
        }
        p++;
    }

    return -1;
}
