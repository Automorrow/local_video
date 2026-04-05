#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "../../include/local_video.h"
#include <stdint.h>

lv_error_t http_server_init(uint16_t port, const char *web_root);
lv_error_t http_server_start(void);
lv_error_t http_server_stop(void);
lv_error_t http_server_close(void);

#endif /* HTTP_SERVER_H */
