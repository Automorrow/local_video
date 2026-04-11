#include "config.h"
#include "../../shared/module/module.h"
#include "../../shared/log/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char db_path_buf[512] = "./local_video.db";
static char web_root_buf[512] = "./web/static";
static char scan_dir_buf[512] = "";

static lv_config_t config = {
    .database_path = db_path_buf,
    .web_root = web_root_buf,
    .http_port = 8080,
    .scan_directory = scan_dir_buf
};

void config_parse_args(int argc, char *argv[])
{
#ifdef _WIN32
    /* No defaults on Windows - user must configure via web UI */
    config.http_port = 0;
    scan_dir_buf[0] = '\0';
#endif

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
            strncpy(scan_dir_buf, argv[i + 1], sizeof(scan_dir_buf) - 1);
            scan_dir_buf[sizeof(scan_dir_buf) - 1] = '\0';
            config.scan_directory = scan_dir_buf;
            i++;
        } else if (strcmp(argv[i], "--db-path") == 0 && i + 1 < argc) {
            strncpy(db_path_buf, argv[i + 1], sizeof(db_path_buf) - 1);
            db_path_buf[sizeof(db_path_buf) - 1] = '\0';
            config.database_path = db_path_buf;
            i++;
        } else if (strcmp(argv[i], "--web-root") == 0 && i + 1 < argc) {
            strncpy(web_root_buf, argv[i + 1], sizeof(web_root_buf) - 1);
            web_root_buf[sizeof(web_root_buf) - 1] = '\0';
            config.web_root = web_root_buf;
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

lv_error_t config_set_scan_directory(const char *dir)
{
    if (!dir || dir[0] == '\0') return LV_ERROR_INVALID_ARG;
    strncpy(scan_dir_buf, dir, sizeof(scan_dir_buf) - 1);
    scan_dir_buf[sizeof(scan_dir_buf) - 1] = '\0';
    config.scan_directory = scan_dir_buf;
    log_info("Config updated: scan_directory = %s", scan_dir_buf);
    return LV_OK;
}

lv_error_t config_set_port(uint16_t port)
{
    if (port == 0) return LV_ERROR_INVALID_ARG;
    config.http_port = port;
    log_info("Config updated: http_port = %d", port);
    return LV_OK;
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
