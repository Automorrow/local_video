#include "config.h"
#include "../../shared/module/module.h"
#include "../../shared/log/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char db_path_buf[512] = "./local_video.db";
static char web_root_buf[512] = "./web/static";
static char scan_dir_buf[512] = "";

#define CONFIG_FILE "local_video.cfg"

static lv_config_t config = {
    .database_path = db_path_buf,
    .web_root = web_root_buf,
    .http_port = 8080,
    .scan_directory = scan_dir_buf
};

static void config_load(void)
{
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (!fp) return;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;

        if (strcmp(key, "http_port") == 0) {
            int port = atoi(val);
            if (port > 0 && port <= 65535) {
                config.http_port = (uint16_t)port;
            }
        } else if (strcmp(key, "scan_directory") == 0) {
            strncpy(scan_dir_buf, val, sizeof(scan_dir_buf) - 1);
            scan_dir_buf[sizeof(scan_dir_buf) - 1] = '\0';
            config.scan_directory = scan_dir_buf;
        } else if (strcmp(key, "database_path") == 0) {
            strncpy(db_path_buf, val, sizeof(db_path_buf) - 1);
            db_path_buf[sizeof(db_path_buf) - 1] = '\0';
            config.database_path = db_path_buf;
        } else if (strcmp(key, "web_root") == 0) {
            strncpy(web_root_buf, val, sizeof(web_root_buf) - 1);
            web_root_buf[sizeof(web_root_buf) - 1] = '\0';
            config.web_root = web_root_buf;
        }
    }
    fclose(fp);
    log_info("Config loaded from %s", CONFIG_FILE);
}

static void config_save(void)
{
    FILE *fp = fopen(CONFIG_FILE, "w");
    if (!fp) {
        log_error("Failed to save config to %s", CONFIG_FILE);
        return;
    }
    fprintf(fp, "http_port=%d\n", config.http_port);
    fprintf(fp, "scan_directory=%s\n", config.scan_directory ? config.scan_directory : "");
    fprintf(fp, "database_path=%s\n", config.database_path ? config.database_path : "");
    fprintf(fp, "web_root=%s\n", config.web_root ? config.web_root : "");
    fclose(fp);
}

void config_parse_args(int argc, char *argv[])
{
    /* Load persisted config from file first */
    config_load();

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
    config_save();
    return LV_OK;
}

lv_error_t config_set_port(uint16_t port)
{
    if (port == 0) return LV_ERROR_INVALID_ARG;
    config.http_port = port;
    log_info("Config updated: http_port = %d", port);
    config_save();
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
