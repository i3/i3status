// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include "i3status.h"

void print_run_watch(yajl_gen json_gen, char *buffer, const char *title, const char *pidfile, const char *format, const char *format_down) {
    bool running = process_runs(pidfile);
    const char *walk;
    char *outwalk = buffer;
    output_color_t outcolor = COLOR_DEFAULT;

    if (running || format_down == NULL) {
        walk = format;
    } else {
        walk = format_down;
    }

    INSTANCE(pidfile);

    outcolor = (running ? COLOR_GOOD : COLOR_BAD);

    for (; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;

        } else if (BEGINS_WITH(walk + 1, "title")) {
            outwalk += sprintf(outwalk, "%s", title);
            walk += strlen("title");

        } else if (BEGINS_WITH(walk + 1, "status")) {
            outwalk += sprintf(outwalk, "%s", (running ? "yes" : "no"));
            walk += strlen("status");

        } else {
            *(outwalk++) = '%';
        }
    }

    OUTPUT_FULL_TEXT(buffer);
}
