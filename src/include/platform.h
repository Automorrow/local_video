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
    #define close _close
    #define lseek _lseek
    #define open _open
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    #define sleep(x) Sleep((x) * 1000)
    #define usleep(x) Sleep((x) / 1000)
    
    /*
     * On Windows, _read/_write only work with file descriptors (MSVCRT),
     * while send/recv only work with sockets (Winsock).
     * We define net_read/net_write for socket I/O, and keep read/write
     * mapped to _read/_write for file I/O.
     *
     * IMPORTANT: Use closesocket() for sockets, _close() for file fds.
     * We cannot macro close() because it would apply to both.
     * Use sock_close() for sockets instead.
     */
    #define read _read
    #define write _write
    #define net_read(fd, buf, len) recv((fd), (buf), (len), 0)
    #define net_write(fd, buf, len) send((fd), (buf), (len), 0)
    #define sock_close(fd) closesocket(fd)
    
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
    
    /* On POSIX, read/write work for both sockets and files */
    #define net_read(fd, buf, len) read((fd), (buf), (len))
    #define net_write(fd, buf, len) write((fd), (buf), (len))
    #define sock_close(fd) close(fd)
#endif

/* Platform initialization */
int platform_init(void);
void platform_cleanup(void);

#endif /* PLATFORM_H */
