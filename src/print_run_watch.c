// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include "i3status.h"

void print_run_watch(yajl_gen json_gen, char *buffer, const char *title, const char *pidfile, const char *format, const char *format_down) {
    bool running = process_runs(pidfile);
    const char *walk;
    char *outwalk = buffer;

    if (running || format_down == NULL) {
        walk = format;
    } else {
        walk = format_down;
    }

    INSTANCE(pidfile);

    START_COLOR((running ? "color_good" : "color_bad"));

    for (; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "title")) {
            outwalk += sprintf(outwalk, "%s", title);
            walk += strlen("title");
        } else if (BEGINS_WITH(walk + 1, "status")) {
            outwalk += sprintf(outwalk, "%s", (running ? "yes" : "no"));
            walk += strlen("status");
        }
    }

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
