#include "platform.h"
#include "log.h"

#ifdef _WIN32

int platform_init(void)
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        log_error("WSAStartup failed: %d", result);
        return -1;
    }
    
    /* Ignore SIGPIPE equivalent on Windows - not needed */
    
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
