#ifndef API_HANDLER_H
#define API_HANDLER_H

#include "local_video.h"
#include "http_request.h"

lv_error_t api_handler_handle(int client_fd, const HttpRequest *req);

#endif /* API_HANDLER_H */
