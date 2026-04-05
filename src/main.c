#include "local_video.h"
#include "module.h"
#include "log.h"
#include "config.h"
#include "http_server.h"
#include "platform.h"
#include <stdio.h>
#include <signal.h>

static volatile int running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
    log_info("Received signal, shutting down...");
    http_server_stop();
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
