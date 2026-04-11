#include "config.h"
#include "../../shared/module/module.h"
#include "../../shared/log/log.h"
#include "../../include/platform.h"
#include "../db_manager/db_manager.h"
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

static int config_db_loaded = 0;

static void config_set_value(const char *key, char *dest_buf, size_t dest_size, const char *default_val)
{
    char tmp[512] = {0};
    if (db_manager_setting_get(key, tmp, sizeof(tmp)) == LV_OK && tmp[0]) {
        snprintf(dest_buf, dest_size, "%s", tmp);
    } else if (default_val) {
        snprintf(dest_buf, dest_size, "%s", default_val);
    }
}

void config_reload_from_db(void)
{
    if (config_db_loaded) return;

    char port_str[16] = {0};
    config_set_value("http_port", port_str, sizeof(port_str), "8080");
    config.http_port = (uint16_t)atoi(port_str);

    config_set_value("scan_directory", scan_dir_buf, sizeof(scan_dir_buf), "");
    config.scan_directory = scan_dir_buf;

    config_set_value("database_path", db_path_buf, sizeof(db_path_buf), "./local_video.db");
    config.database_path = db_path_buf;

    config_set_value("web_root", web_root_buf, sizeof(web_root_buf), "./web/static");
    config.web_root = web_root_buf;

    config_db_loaded = 1;
    log_info("Config loaded from database");
}

static void config_persist_value(const char *key, const char *value)
{
    db_manager_setting_set(key, value);
}

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
    config_persist_value("scan_directory", scan_dir_buf);
    return LV_OK;
}

lv_error_t config_set_port(uint16_t port)
{
    if (port == 0) return LV_ERROR_INVALID_ARG;
    config.http_port = port;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", (int)port);
    log_info("Config updated: http_port = %d", port);
    config_persist_value("http_port", port_str);
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

#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int config_path_allowed(const char *path)
{
    if (!path || path[0] == '\0') return 0;
    if (!scan_dir_buf[0]) return 0;

    char base_norm[PATH_MAX] = {0};
    char path_norm[PATH_MAX] = {0};

#ifdef _WIN32
    if (GetFullPathNameA(scan_dir_buf, sizeof(base_norm), base_norm, NULL) == 0) return 0;
    if (GetFullPathNameA(path, sizeof(path_norm), path_norm, NULL) == 0) return 0;
#else
    if (!realpath(scan_dir_buf, base_norm)) return 0;
    if (!realpath(path, path_norm)) return 0;
#endif

    size_t base_len = strlen(base_norm);
    size_t path_len = strlen(path_norm);

    while (base_len > 0 && (base_norm[base_len - 1] == '/' || base_norm[base_len - 1] == '\\')) {
        base_norm[--base_len] = '\0';
    }
    while (path_len > 0 && (path_norm[path_len - 1] == '/' || path_norm[path_len - 1] == '\\')) {
        path_norm[--path_len] = '\0';
    }

#ifdef _WIN32
    if (_strnicmp(path_norm, base_norm, base_len) != 0) return 0;
#else
    if (strncmp(path_norm, base_norm, base_len) != 0) return 0;
#endif

    if (path_len == base_len) return 1;
    char next = path_norm[base_len];
    if (next == '/' || next == '\\') return 1;
    return 0;
}

MODULE_INIT(config_init, "config");
MODULE_EXIT(config_exit, "config");
