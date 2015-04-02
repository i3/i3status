// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <sys/stat.h>
#include "i3status.h"

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

    for (; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "title")) {
            outwalk += sprintf(outwalk, "%s", title);
            walk += strlen("title");
        } else if (BEGINS_WITH(walk + 1, "status")) {
            outwalk += sprintf(outwalk, "%s", (exists ? "yes" : "no"));
            walk += strlen("status");
        }
    }

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
