#include "http_server_internal.h"
#include "http_response.h"
#include "../../shared/log/log.h"
#include "../../include/platform.h"
#include <errno.h>
#include <string.h>

lv_error_t http_server_serve_video_stream(int client_fd,
                                          const HttpRequest *req,
                                          const char *video_path)
{
    lv_error_t err;
    int enable_range;

    if (!req || !video_path) {
        return LV_ERROR_INVALID_ARG;
    }

    log_info("Serving video: %s", video_path);

    enable_range = (req->range_start >= 0 || req->range_end >= 0);
    err = http_server_send_file_response(client_fd,
                                         video_path,
                                         req->range_start,
                                         req->range_end,
                                         enable_range);

    if (err == LV_OK) {
        log_info("Video served successfully");
    } else if (SOCKET_ERRNO == EPIPE_W || SOCKET_ERRNO == ECONNRESET_W) {
        log_debug("Client disconnected during video stream");
    } else {
        log_error("Failed to serve video: %s", video_path);
    }

    return err;
}
