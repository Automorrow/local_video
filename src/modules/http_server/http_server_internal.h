#ifndef HTTP_SERVER_INTERNAL_H
#define HTTP_SERVER_INTERNAL_H

#include "http_request.h"
#include "../../include/local_video.h"
#include <stdint.h>

const char *http_server_get_mime_type(const char *path);
lv_error_t http_server_send_file_response(int client_fd,
                                          const char *file_path,
                                          int64_t range_start,
                                          int64_t range_end,
                                          int enable_range);
lv_error_t http_server_serve_static_file(int client_fd,
                                         const char *path,
                                         const char *web_root);
lv_error_t http_server_serve_thumbnail(int client_fd, int64_t video_id);
lv_error_t http_server_serve_video_stream(int client_fd,
                                          const HttpRequest *req,
                                          const char *video_path);

#endif
