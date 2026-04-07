cmake_minimum_required(VERSION 3.10)
project(LocalVideoServer C)

enable_testing()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform detection
if(WIN32 OR CMAKE_CROSSCOMPILING)
    set(IS_WINDOWS TRUE)
elseif(DEFINED ENV{MSYSTEM} AND "$ENV{MSYSTEM}" MATCHES "MINGW")
    set(IS_WINDOWS TRUE)
else()
    set(IS_WINDOWS FALSE)
endif()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
elseif(IS_WINDOWS)
    # MinGW cross-compile - less strict
    add_compile_options(
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-format-truncation
    )
else()
    # Linux native
    add_compile_options(
        -Wall
        -Wextra
        -Werror
        -Wshadow
        -Wpointer-arith
        -Wcast-qual
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wmissing-declarations
        -Wno-format-truncation
    )
endif()

# Platform abstraction
if(IS_WINDOWS)
    add_definitions(-D_WIN32_WINNT=0x0601)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    # pthread-win32 for Windows
    set(PLATFORM_LIBS ws2_32 winmm pthread)
else()
    set(PLATFORM_LIBS pthread)
endif()

# Core sources (common to both platforms)
set(CORE_SOURCES
    src/main.c
    src/shared/module/module.c
    src/shared/notifier/notifier.c
    src/shared/log/log.c
    src/shared/thread/thread.c
    src/shared/json/json.c
    src/shared/platform/platform.c
    src/modules/config/config.c
    src/modules/db_manager/db_manager.c
    src/modules/db_manager/db_manager_video.c
    src/modules/db_manager/db_manager_user.c
    src/modules/db_manager/db_manager_blacklist.c
    src/modules/http_server/http_server.c
    src/modules/http_server/http_server_file.c
    src/modules/http_server/http_server_thumbnail.c
    src/modules/http_server/http_server_stream.c
    src/modules/http_server/http_request.c
    src/modules/http_server/http_response.c
    src/modules/api_handler/api_handler.c
    src/modules/api_handler/api_response.c
    src/modules/api_handler/api_utils.c
    src/modules/api_handler/api_endpoints.c
)

# Linux-only sources
set(LINUX_SOURCES
    src/modules/video_scanner/video_scanner.c
    src/modules/video_scanner/video_scanner_watcher.c
    src/modules/video_scanner/video_scanner_scan.c
)

# Include directories
include_directories(
    src/include
    src/shared/module
    src/shared/list
    src/shared/notifier
    src/shared/log
    src/shared/thread
    src/shared/json
    src/shared/platform
    src/modules/config
    src/modules/db_manager
    src/modules/http_server
    src/modules/video_scanner
    src/modules/api_handler
)

# SQLite3
if(IS_WINDOWS)
    # For Windows cross-compile, use bundled sqlite3
    include_directories(third_party/sqlite3)
    link_directories(third_party/sqlite3)
    set(SQLITE3_LIB sqlite3)
else()
    # Linux native - use system sqlite3
    find_package(SQLite3 REQUIRED)
    set(SQLITE3_LIB SQLite::SQLite3)
endif()

# Main executable
if(IS_WINDOWS)
    # Windows: core sources only (no video_scanner)
    add_executable(local_video ${CORE_SOURCES})
    target_compile_definitions(local_video PRIVATE WINDOWS_BUILD)
    # Windows doesn't support linker sections the same way
    set_target_properties(local_video PROPERTIES
        LINK_FLAGS "-Wl,--allow-multiple-definition"
    )
else()
    # Linux: full sources
    add_executable(local_video ${CORE_SOURCES} ${LINUX_SOURCES})
endif()

# Linker options
if(NOT IS_WINDOWS)
    target_link_options(local_video PRIVATE -T ${CMAKE_SOURCE_DIR}/linker.ld)
else()
    target_link_options(local_video PRIVATE -T ${CMAKE_SOURCE_DIR}/linker-mingw.ld)
endif()

# Link libraries
target_link_libraries(local_video ${PLATFORM_LIBS} ${SQLITE3_LIB})

set_target_properties(local_video PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Tests (Linux only)
if(NOT IS_WINDOWS)
    function(add_lv_test target)
        add_executable(${target} ${ARGN})
        target_link_options(${target} PRIVATE -T ${CMAKE_SOURCE_DIR}/linker.ld)
        target_link_libraries(${target} pthread sqlite3)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
    endfunction()

    add_lv_test(test_module tests/unit/test_module.c src/shared/module/module.c)
    add_lv_test(test_list tests/unit/test_list.c)
    add_lv_test(test_notifier tests/unit/test_notifier.c src/shared/notifier/notifier.c)
    add_lv_test(test_log tests/unit/test_log.c src/shared/log/log.c)
    add_lv_test(test_thread tests/unit/test_thread.c src/shared/thread/thread.c)
    add_lv_test(test_json tests/unit/test_json.c src/shared/json/json.c)
    add_lv_test(test_db tests/unit/test_db_manager.c 
        src/modules/db_manager/db_manager.c
        src/modules/db_manager/db_manager_video.c
        src/modules/db_manager/db_manager_user.c
        src/modules/db_manager/db_manager_blacklist.c
        src/modules/config/config.c
        src/shared/thread/thread.c
        src/shared/log/log.c
    )
    add_lv_test(test_video_scanner tests/unit/test_video_scanner.c
        src/modules/video_scanner/video_scanner.c
        src/modules/video_scanner/video_scanner_watcher.c
        src/modules/video_scanner/video_scanner_scan.c
        src/modules/db_manager/db_manager.c
        src/modules/db_manager/db_manager_video.c
        src/modules/db_manager/db_manager_user.c
        src/modules/db_manager/db_manager_blacklist.c
        src/modules/config/config.c
        src/shared/module/module.c
        src/shared/thread/thread.c
        src/shared/log/log.c
    )

    add_test(NAME test_module COMMAND test_module WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_test(NAME test_list COMMAND test_list WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_test(NAME test_notifier COMMAND test_notifier WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_test(NAME test_log COMMAND test_log WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_test(NAME test_thread COMMAND test_thread WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_test(NAME test_json COMMAND test_json WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_test(NAME test_db COMMAND test_db WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_test(NAME test_video_scanner COMMAND test_video_scanner WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()
