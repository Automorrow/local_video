# AGENTS.md - LocalVideoServer Development Guide

This guide is for AI coding agents working on the LocalVideoServer codebase.

## Project Overview

LocalVideoServer is a lightweight local video server written in C99 with SQLite3 database, custom HTTP/1.1 server, and a modular architecture using linker sections for automatic module registration.

**Tech Stack**: C99, SQLite3, CMake, pthreads, custom HTTP server

## Primary References

Before changing code, prefer these repository documents as the source of truth:

- `README.md` - project overview and entrypoints
- `specs/项目结构.md` - repository layout and placement rules
- `specs/开发规范.md` - coding standards and repository boundary rules
- `docs/模块系统.md` - linker-based module registration mechanism

This file is an agent-facing operating guide. Do not treat it as the authoritative replacement for the specs.

## Build Commands

### Full Build
```bash
mkdir -p build
cd build
cmake ..
make
```

### Build Single Target
```bash
cd build
make local_video          # Main application
make test_module          # Module system tests
make test_list            # List data structure tests
make test_notifier        # Notifier chain tests
make test_log             # Logging system tests
make test_thread          # Thread utilities tests
make test_json            # JSON writer tests
make test_db              # Database manager tests
```

### Run Single Test
```bash
cd build/bin
./test_module             # Run specific test binary
./test_list
./test_db
# etc.
```

### Clean Build
```bash
cd build
make clean
rm -rf *
cmake ..
make
```

## Running the Application

```bash
cd build/bin
./local_video --port 8080 --video-dir /path/to/videos
```

## Code Style Guidelines

### File Organization

**Header Guards**: Use `#ifndef HEADER_H` / `#define HEADER_H` / `#endif` pattern
**Include Order**:
1. Project headers (local_video.h first if needed)
2. Module-specific headers
3. Standard library headers (stdio.h, stdlib.h, string.h, etc.)
4. System headers (pthread.h, sys/socket.h, etc.)

**Example**:
```c
#include "local_video.h"
#include "module.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
```

### Naming Conventions

**Functions**: `snake_case` with module prefix
- `log_info()`, `http_server_start()`, `db_manager_init()`

**Types**: `snake_case_t` suffix for typedefs
- `lv_error_t`, `json_writer_t`, `list_node_t`

**Enums**: `UPPER_CASE` with module prefix
- `LV_OK`, `LV_ERROR_MEMORY`, `LV_LOG_INFO`

**Macros**: `UPPER_CASE`
- `MODULE_INIT()`, `TEST_ASSERT()`, `container_of()`

**Static functions**: `snake_case` (no prefix needed)
- `static void signal_handler(int sig)`

**Global variables**: `snake_case` with `static` when file-scoped
- `static volatile int server_running = 0;`

### Types and Declarations

**Standard Types**: Use `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`
- `uint16_t`, `int32_t`, `size_t`, `bool`, `true`, `false`

**Error Handling**: Return `lv_error_t` for functions that can fail
```c
typedef enum {
    LV_OK = 0,
    LV_ERROR_INVALID_ARG,
    LV_ERROR_MEMORY,
    LV_ERROR_IO,
    LV_ERROR_DB,
    LV_ERROR_NETWORK,
    LV_ERROR_UNKNOWN
} lv_error_t;
```

**Struct Definitions**: Use typedef with `_t` suffix
```c
typedef struct {
    const char *name;
    void (*fn)(void);
} module_init_entry_t;
```

### Memory Management

**Allocation**: Check return values immediately
```c
char *buffer = malloc(size);
if (!buffer) {
    return LV_ERROR_MEMORY;
}
```

**Cleanup**: Always free allocated memory, close file descriptors
```c
if (fd >= 0) {
    close(fd);
}
free(buffer);
```

**SQLite**: Use `sqlite3_free()` for SQLite-allocated memory

### Error Handling

**Check all system calls and library functions**:
```c
if (pthread_create(&thread, NULL, worker, NULL) != 0) {
    log_error("Failed to create thread");
    return LV_ERROR_UNKNOWN;
}
```

**Logging**: Use appropriate log levels
- `log_debug()` - Detailed diagnostic information
- `log_info()` - General informational messages
- `log_warning()` - Warning conditions
- `log_error()` - Error conditions

### Module System

This project uses a custom module system with linker sections for automatic registration.

**Module Registration Macros**:
```c
MODULE_INIT(function_name, "module_name");  // Initialization phase
MODULE_SUB(function_name, "module_name");   // Subscription phase
MODULE_RUN(function_name, "module_name");   // Run phase
MODULE_EXIT(function_name, "module_name");  // Exit phase
```

**Module Lifecycle**:
1. `module_init_all()` - Initialize all modules
2. `module_sub_all()` - Subscribe modules to events
3. `module_run_all()` - Start module execution
4. `module_exit_all()` - Clean shutdown

**Example Module**:
```c
static void my_module_init(void) {
    log_info("Initializing my_module");
}

static void my_module_run(void) {
    log_info("Running my_module");
}

static void my_module_exit(void) {
    log_info("Exiting my_module");
}

MODULE_INIT(my_module_init, "my_module");
MODULE_RUN(my_module_run, "my_module");
MODULE_EXIT(my_module_exit, "my_module");
```

### Compiler Warnings

The project uses strict warning flags (`-Wall -Wextra -Werror`). Code must compile without warnings.

**Common issues to avoid**:
- Unused variables: Remove or cast to `(void)variable`
- Implicit function declarations: Include proper headers
- Format string mismatches: Use correct format specifiers
- Shadowing: Don't reuse variable names in nested scopes

### Thread Safety

**Shared State**: Protect with mutexes or use atomic operations
**Signal Handlers**: Use `volatile sig_atomic_t` for flags modified in signal handlers
```c
static volatile int running = 1;
```

### Testing

**Test Structure**: Each test file should include:
- Test assertion macro
- Individual test functions
- `main()` that runs all tests and reports results

**Test Pattern**:
```c
#define TEST_ASSERT(cond, msg) \
    do { \
        test_count++; \
        if (cond) { \
            printf("  [PASS] %s\n", msg); \
        } else { \
            printf("  [FAIL] %s\n", msg); \
            test_results++; \
        } \
    } while (0)
```

## Important Notes

- **C Standard**: Strict C99 compliance required
- **Linker Script**: Custom `linker.ld` used for module sections
- **Database**: SQLite3 for persistent storage
- **Threading**: POSIX threads (pthread) for concurrency
- **HTTP**: Custom HTTP/1.1 implementation (no external libraries)
- **Security**: Path traversal prevention in static file serving
- **Signals**: Graceful shutdown on SIGINT/SIGTERM, ignore SIGPIPE

## Common Patterns

**Intrusive Linked Lists**: Use `list.h` for kernel-style linked lists
**JSON Output**: Use `json_writer_t` for streaming JSON generation
**Configuration**: Use `config.h` for command-line argument parsing
**Logging**: Always use `log_*()` functions, never raw `printf()`
