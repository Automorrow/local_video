#include "json.h"
#include <string.h>
#include <stdio.h>

lv_error_t json_writer_init(json_writer_t *writer, FILE *file)
{
    if (!writer || !file) {
        return LV_ERROR_INVALID_ARG;
    }

    writer->file = file;
    writer->first = 1;
    writer->in_array = 0;
    return LV_OK;
}

static void json_add_separator(json_writer_t *writer)
{
    if (!writer->first) {
        fprintf(writer->file, ",");
    }
    writer->first = 0;
}

lv_error_t json_start_object(json_writer_t *writer)
{
    if (!writer) {
        return LV_ERROR_INVALID_ARG;
    }

    json_add_separator(writer);
    fprintf(writer->file, "{");
    writer->first = 1;
    return LV_OK;
}

lv_error_t json_end_object(json_writer_t *writer)
{
    if (!writer) {
        return LV_ERROR_INVALID_ARG;
    }

    fprintf(writer->file, "}");
    writer->first = 0;
    return LV_OK;
}

lv_error_t json_start_array(json_writer_t *writer)
{
    if (!writer) {
        return LV_ERROR_INVALID_ARG;
    }

    json_add_separator(writer);
    fprintf(writer->file, "[");
    writer->first = 1;
    return LV_OK;
}

lv_error_t json_end_array(json_writer_t *writer)
{
    if (!writer) {
        return LV_ERROR_INVALID_ARG;
    }

    fprintf(writer->file, "]");
    writer->first = 0;
    return LV_OK;
}

lv_error_t json_add_key(json_writer_t *writer, const char *key)
{
    if (!writer || !key) {
        return LV_ERROR_INVALID_ARG;
    }

    json_add_separator(writer);
    fprintf(writer->file, "\"%s\":", key);
    writer->first = 1;
    return LV_OK;
}

static void json_escape_string_to_file(FILE *f, const char *str)
{
    while (*str) {
        char c = *str++;
        switch (c) {
            case '"':
                fprintf(f, "\\\"");
                break;
            case '\\':
                fprintf(f, "\\\\");
                break;
            case '\b':
                fprintf(f, "\\b");
                break;
            case '\f':
                fprintf(f, "\\f");
                break;
            case '\n':
                fprintf(f, "\\n");
                break;
            case '\r':
                fprintf(f, "\\r");
                break;
            case '\t':
                fprintf(f, "\\t");
                break;
            default:
                fprintf(f, "%c", c);
                break;
        }
    }
}

lv_error_t json_add_string(json_writer_t *writer, const char *value)
{
    if (!writer || !value) {
        return LV_ERROR_INVALID_ARG;
    }

    json_add_separator(writer);
    fprintf(writer->file, "\"");
    json_escape_string_to_file(writer->file, value);
    fprintf(writer->file, "\"");
    return LV_OK;
}

lv_error_t json_escape_string(const char *input, char *output, size_t output_size)
{
    size_t pos = 0;

    if (!input || !output || output_size == 0) {
        return LV_ERROR_INVALID_ARG;
    }

    while (*input) {
        unsigned char c = (unsigned char)*input++;
        const char *replacement = NULL;
        char unicode_buf[7];
        size_t replacement_len;

        switch (c) {
            case '"': replacement = "\\\""; break;
            case '\\': replacement = "\\\\"; break;
            case '\b': replacement = "\\b"; break;
            case '\f': replacement = "\\f"; break;
            case '\n': replacement = "\\n"; break;
            case '\r': replacement = "\\r"; break;
            case '\t': replacement = "\\t"; break;
            default:
                if (c < 0x20) {
                    snprintf(unicode_buf, sizeof(unicode_buf), "\\u%04x", c);
                    replacement = unicode_buf;
                }
                break;
        }

        if (!replacement) {
            if (pos + 1 >= output_size) {
                return LV_ERROR_MEMORY;
            }
            output[pos++] = (char)c;
            continue;
        }

        replacement_len = strlen(replacement);
        if (pos + replacement_len >= output_size) {
            return LV_ERROR_MEMORY;
        }

        memcpy(output + pos, replacement, replacement_len);
        pos += replacement_len;
    }

    output[pos] = '\0';
    return LV_OK;
}

lv_error_t json_add_int(json_writer_t *writer, int value)
{
    if (!writer) {
        return LV_ERROR_INVALID_ARG;
    }

    json_add_separator(writer);
    fprintf(writer->file, "%d", value);
    return LV_OK;
}

lv_error_t json_add_bool(json_writer_t *writer, bool value)
{
    if (!writer) {
        return LV_ERROR_INVALID_ARG;
    }

    json_add_separator(writer);
    fprintf(writer->file, "%s", value ? "true" : "false");
    return LV_OK;
}

lv_error_t json_add_null(json_writer_t *writer)
{
    if (!writer) {
        return LV_ERROR_INVALID_ARG;
    }

    json_add_separator(writer);
    fprintf(writer->file, "null");
    return LV_OK;
}
