#include "config.h"
#include "../../shared/module/module.h"
#include "../../shared/log/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_config_t config = {
    .database_path = "./local_video.db",
    .web_root = "./web/static",
    .http_port = 8080,
    .scan_directory = "./videos"
};

void config_parse_args(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            char *endptr;
            long port = strtol(argv[i + 1], &endptr, 10);
            if (*endptr == '\0' && port > 0 && port <= 65535) {
                config.http_port = (uint16_t)port;
            } else {
                log_warning("Invalid port '%s', using default %d", argv[i + 1], config.http_port);
            }
            i++;
        } else if (strcmp(argv[i], "--video-dir") == 0 && i + 1 < argc) {
            static char scan_dir[256];
            strncpy(scan_dir, argv[i + 1], sizeof(scan_dir) - 1);
            scan_dir[sizeof(scan_dir) - 1] = '\0';
            config.scan_directory = scan_dir;
            i++;
        } else if (strcmp(argv[i], "--db-path") == 0 && i + 1 < argc) {
            static char db_path[256];
            strncpy(db_path, argv[i + 1], sizeof(db_path) - 1);
            db_path[sizeof(db_path) - 1] = '\0';
            config.database_path = db_path;
            i++;
        } else if (strcmp(argv[i], "--web-root") == 0 && i + 1 < argc) {
            static char web_root[256];
            strncpy(web_root, argv[i + 1], sizeof(web_root) - 1);
            web_root[sizeof(web_root) - 1] = '\0';
            config.web_root = web_root;
            i++;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --port PORT       Set HTTP server port (default: 8080)\n");
            printf("  --video-dir DIR   Set video directory to scan (default: ./videos)\n");
            printf("  --db-path PATH    Set database file path (default: ./local_video.db)\n");
            printf("  --web-root DIR    Set web root directory (default: ./web/static)\n");
            printf("  --help, -h        Show this help message\n");
            exit(0);
        } else {
            log_warning("Unknown argument: %s", argv[i]);
        }
    }
}

const lv_config_t *config_get(void)
{
    return &config;
}

static void config_init(void)
{
    log_info("Config module initialized");
    log_info("  Database path: %s", config.database_path);
    log_info("  Web root: %s", config.web_root);
    log_info("  HTTP port: %d", config.http_port);
    log_info("  Scan directory: %s", config.scan_directory);
}

static void config_exit(void)
{
    log_info("Config module exited");
}

MODULE_INIT(config_init, "config");
MODULE_EXIT(config_exit, "config");
