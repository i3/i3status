// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <sys/stat.h>
#include "i3status.h"

#define STRING_SIZE 5

void print_path_exists(yajl_gen json_gen, char *buffer, const char *title, const char *path, const char *format, const char *format_down) {
    const char *walk;
    char *outwalk = buffer;
    struct stat st;
    const bool exists = (stat(path, &st) == 0);

    if (exists || format_down == NULL) {
        walk = format;
    } else {
        walk = format_down;
    }

    INSTANCE(path);

    START_COLOR((exists ? "color_good" : "color_bad"));

    char string_status[STRING_SIZE];

    snprintf(string_status, STRING_SIZE, "%s", (exists ? "yes" : "no"));

    placeholder_t placeholders[] = {
        {.name = "%title", .value = title},
        {.name = "%status", .value = string_status}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    buffer = format_placeholders(walk, &placeholders[0], num);

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
    free(buffer);
}
