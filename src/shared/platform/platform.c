#include "../../include/platform.h"
#include "../log/log.h"

#ifdef _WIN32

int platform_init(void)
{
    /* Set console to UTF-8 for proper Chinese output */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        log_error("WSAStartup failed: %d", result);
        return -1;
    }

    return 0;
}

void platform_cleanup(void)
{
    WSACleanup();
}

#else /* POSIX */

int platform_init(void)
{
    /* Ignore SIGPIPE to prevent crashes on broken pipe */
    signal(SIGPIPE, SIG_IGN);
    return 0;
}

void platform_cleanup(void)
{
    /* Nothing to do on POSIX */
}

#endif
