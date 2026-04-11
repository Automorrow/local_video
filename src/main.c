#include "include/local_video.h"
#include "shared/module/module.h"
#include "shared/log/log.h"
#include "modules/config/config.h"
#include "modules/http_server/http_server.h"
#include "include/platform.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

static volatile int running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

static void open_browser(const char *url)
{
#ifdef _WIN32
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#else
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "xdg-open %s &", url);
    int ret = system(cmd);
    (void)ret;
#endif
}

int main(int argc, char *argv[])
{
    /* Initialize platform (Winsock on Windows) */
    if (platform_init() != 0) {
        fprintf(stderr, "Failed to initialize platform\n");
        return 1;
    }

    /* Parse command line args first */
    config_parse_args(argc, argv);

    log_set_level(LV_LOG_INFO);
    log_info("Local Video Server starting...");

    /* Set up signal handlers for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN); /* Ignore SIGPIPE to prevent crash on client disconnect */
#endif

    log_info("Initializing modules...");
    module_init_all();

    log_info("Subscribing modules...");
    module_sub_all();

    log_info("Running modules...");
    module_run_all();

    log_info("Server is running... Press Ctrl+C to stop.");

    /* Wait for HTTP server to be ready (port assigned if it was 0) */
    http_server_wait_ready();

    /* Auto-open browser to main page */
    {
        const lv_config_t *cfg = config_get();
        char url[256];
        snprintf(url, sizeof(url), "http://127.0.0.1:%d/", (int)cfg->http_port);
        log_info("Opening browser: %s", url);
        open_browser(url);
    }

    /* Main loop - keep the server running */
    while (running) {
        sleep(1);
    }

    log_info("Exiting modules...");
    module_exit_all();

    platform_cleanup();

    log_info("Local Video Server stopped.");

    return 0;
}
