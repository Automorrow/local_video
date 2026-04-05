#ifndef JSON_H
#define JSON_H

#include "../../include/local_video.h"
#include <stdio.h>

typedef struct {
    FILE *file;
    int first;
    int in_array;
} json_writer_t;

lv_error_t json_writer_init(json_writer_t *writer, FILE *file);

lv_error_t json_start_object(json_writer_t *writer);
lv_error_t json_end_object(json_writer_t *writer);

lv_error_t json_start_array(json_writer_t *writer);
lv_error_t json_end_array(json_writer_t *writer);

lv_error_t json_add_key(json_writer_t *writer, const char *key);

lv_error_t json_add_string(json_writer_t *writer, const char *value);
lv_error_t json_add_int(json_writer_t *writer, int value);
lv_error_t json_add_bool(json_writer_t *writer, bool value);
lv_error_t json_add_null(json_writer_t *writer);
lv_error_t json_escape_string(const char *input, char *output, size_t output_size);

#endif /* JSON_H */
