#ifndef CONFIG_H
#define CONFIG_H

#include "../../include/local_video.h"

typedef struct {
    const char *database_path;
    const char *web_root;
    uint16_t http_port;
    const char *scan_directory;
} lv_config_t;

const lv_config_t *config_get(void);
void config_parse_args(int argc, char *argv[]);
void config_reload_from_db(void);
lv_error_t config_set_scan_directory(const char *dir);
lv_error_t config_set_port(uint16_t port);
int config_path_allowed(const char *path);

#endif /* CONFIG_H */
