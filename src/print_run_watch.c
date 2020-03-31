// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include "i3status.h"

#define STRING_SIZE 5

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

    char string_status[STRING_SIZE];
    snprintf(string_status, STRING_SIZE, "%s", (running ? "yes" : "no"));

    placeholder_t placeholders[] = {
        {.name = "%title", .value = title},
        {.name = "%status", .value = string_status}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    buffer = format_placeholders(walk, &placeholders[0], num);

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
