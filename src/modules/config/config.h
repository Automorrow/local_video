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

#endif /* CONFIG_H */
