#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
    /* Windows specific */
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
    #include <sys/stat.h>
    
    /* Windows compatibility macros */
    #define close(fd) _close(fd)
    #define read _read
    #define write _write
    #define lseek _lseek
    #define open _open
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    #define sleep(x) Sleep((x) * 1000)
    #define usleep(x) Sleep((x) / 1000)
    
    /* Windows socket compatibility */
    #define SHUT_RDWR SD_BOTH
    #define SHUT_WR SD_SEND
    
    typedef int socklen_t;
    
    /* Windows doesn't have these signals */
    #define SIGPIPE 13
    #define SIG_IGN ((void (*)(int))1)
    
    /* Windows stat compatibility - use _stat64 for large file support */
    #define stat _stat64
    #define fstat _fstat64
    
    /* lstat doesn't exist on Windows, use stat instead */
    #define lstat(path, buf) stat(path, buf)
    
    /* S_ISLNK always false on Windows */
    #ifndef S_ISLNK
        #define S_ISLNK(mode) (0)
    #endif
    
    /* Ensure S_ISREG is defined */
    #ifndef S_ISREG
        #define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
    #endif
    
    /* Ensure S_ISDIR is defined */
    #ifndef S_ISDIR
        #define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
    #endif
    
#else
    /* POSIX / Linux */
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <sys/stat.h>
    #include <strings.h>
    #include <signal.h>
    #include <pthread.h>
#endif

/* Platform initialization */
int platform_init(void);
void platform_cleanup(void);

#endif /* PLATFORM_H */
