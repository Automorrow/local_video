#include "http_server_internal.h"
#include "../db_manager/db_manager.h"
#include "http_response.h"
#include "../../include/platform.h"
#include <stdio.h>
#include <string.h>

static const char *image_extensions[] = {".jpg", ".jpeg", ".png", ".gif", ".webp", NULL};

static int file_exists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void find_preview_image(const char *dir_path,
                               char *preview_path,
                               size_t path_size)
{
    char base_path[1024];
    int i;

    preview_path[0] = '\0';
    if (!dir_path || path_size == 0) {
        return;
    }

    for (i = 0; image_extensions[i]; i++) {
        snprintf(base_path, sizeof(base_path), "%s/preview%s", dir_path, image_extensions[i]);
        if (file_exists(base_path)) {
            size_t len = strlen(base_path);
            if (len >= path_size) {
                len = path_size - 1;
            }
            memcpy(preview_path, base_path, len);
            preview_path[len] = '\0';
            return;
        }
    }
}

static void find_video_thumbnail(const char *video_path,
                                 char *thumb_path,
                                 size_t path_size)
{
    const char *last_slash;
    const char *basename;
    size_t dir_len;
    char dir[1024] = {0};
    const char *ext;
    size_t name_len;
    char base_name[513];
    char full_path[2048];
    int i;

    thumb_path[0] = '\0';
    if (!video_path || path_size == 0) {
        return;
    }

    last_slash = strrchr(video_path, '/');
    basename = last_slash ? last_slash + 1 : video_path;
    dir_len = last_slash ? (size_t)(last_slash - video_path) : 0;

    if (dir_len > 0) {
        if (dir_len >= sizeof(dir)) {
            dir_len = sizeof(dir) - 1;
        }
        strncpy(dir, video_path, dir_len);
        dir[dir_len] = '\0';
    } else {
        strcpy(dir, ".");
    }

    ext = strrchr(basename, '.');
    name_len = ext ? (size_t)(ext - basename) : strlen(basename);
    if (name_len > 512) {
        name_len = 512;
    }

    memcpy(base_name, basename, name_len);
    base_name[name_len] = '\0';

    for (i = 0; image_extensions[i]; i++) {
        snprintf(full_path, sizeof(full_path), "%s/%s%s", dir, base_name, image_extensions[i]);
        if (file_exists(full_path)) {
            size_t len = strlen(full_path);
            if (len >= path_size) {
                len = path_size - 1;
            }
            memcpy(thumb_path, full_path, len);
            thumb_path[len] = '\0';
            return;
        }
    }
}

static void get_video_directory(const char *video_path,
                                char *video_dir,
                                size_t video_dir_size)
{
    const char *last_slash = strrchr(video_path, '/');
    size_t dir_len = last_slash ? (size_t)(last_slash - video_path) : 0;

    if (dir_len > 0) {
        if (dir_len >= video_dir_size) {
            dir_len = video_dir_size - 1;
        }
        memcpy(video_dir, video_path, dir_len);
        video_dir[dir_len] = '\0';
    } else {
        strncpy(video_dir, ".", video_dir_size - 1);
        video_dir[video_dir_size - 1] = '\0';
    }
}

static void find_imgs_thumbnail(const char *video_dir,
                                const char *basename,
                                char *parent_dir,
                                size_t parent_dir_size,
                                char *imgs_path,
                                size_t imgs_path_size)
{
    const char *video_slash = strstr(video_dir, "/video");
    char base_name_noext[256] = {0};
    const char *ext = strrchr(basename, '.');
    size_t name_len = ext ? (size_t)(ext - basename) : strlen(basename);
    int i;

    parent_dir[0] = '\0';
    imgs_path[0] = '\0';

    if (!video_slash) {
        return;
    }

    if ((size_t)(video_slash - video_dir) >= parent_dir_size) {
        return;
    }

    memcpy(parent_dir, video_dir, (size_t)(video_slash - video_dir));
    parent_dir[video_slash - video_dir] = '\0';

    if (name_len >= sizeof(base_name_noext)) {
        name_len = sizeof(base_name_noext) - 1;
    }
    strncpy(base_name_noext, basename, name_len);
    base_name_noext[name_len] = '\0';

    {
        char *test_suffix = strstr(base_name_noext, "-test");
        if (test_suffix) {
            *test_suffix = '\0';
        }
    }

    for (i = 0; image_extensions[i]; i++) {
        char imgs_dir[2048];

        snprintf(imgs_dir, sizeof(imgs_dir), "%s/imgs", parent_dir);
        snprintf(imgs_path, imgs_path_size, "%s/%s%s", imgs_dir, base_name_noext, image_extensions[i]);
        if (file_exists(imgs_path)) {
            return;
        }
        imgs_path[0] = '\0';
    }
}

lv_error_t http_server_serve_thumbnail(int client_fd, int64_t video_id)
{
    VideoInfo video;
    struct stat st;
    char preview_path[1024] = {0};
    char thumb_path[1024] = {0};
    char imgs_path[2048] = {0};
    char video_dir[1024] = {0};
    char parent_dir[1024] = {0};
    const char *last_slash;
    const char *basename;

    if (db_manager_video_get_by_id(video_id, &video) != LV_OK) {
        http_response_send_error(client_fd, 404, "Thumbnail not found");
        return LV_ERROR_IO;
    }

    get_video_directory(video.path, video_dir, sizeof(video_dir));

    last_slash = strrchr(video.path, '/');
    basename = last_slash ? last_slash + 1 : video.path;

    find_imgs_thumbnail(video_dir,
                        basename,
                        parent_dir,
                        sizeof(parent_dir),
                        imgs_path,
                        sizeof(imgs_path));
    if (imgs_path[0] != '\0' && stat(imgs_path, &st) == 0) {
        return http_server_send_file_response(client_fd, imgs_path, 0, 0, 0);
    }

    find_preview_image(video_dir, preview_path, sizeof(preview_path));
    if (preview_path[0] != '\0' && stat(preview_path, &st) == 0) {
        return http_server_send_file_response(client_fd, preview_path, 0, 0, 0);
    }

    if (parent_dir[0] != '\0') {
        find_preview_image(parent_dir, preview_path, sizeof(preview_path));
        if (preview_path[0] != '\0' && stat(preview_path, &st) == 0) {
            return http_server_send_file_response(client_fd, preview_path, 0, 0, 0);
        }
    }

    find_video_thumbnail(video.path, thumb_path, sizeof(thumb_path));
    if (thumb_path[0] != '\0' && stat(thumb_path, &st) == 0) {
        return http_server_send_file_response(client_fd, thumb_path, 0, 0, 0);
    }

    http_response_send_error(client_fd, 404, "Thumbnail not found");
    return LV_ERROR_IO;
}
