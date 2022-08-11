// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include "i3status.h"

#define STRING_SIZE 5

void print_run_watch(run_watch_ctx_t *ctx) {
    bool running = process_runs(ctx->pidfile);
    const char *walk;
    char *outwalk = ctx->buf;

    if (running || ctx->format_down == NULL) {
        walk = ctx->format;
    } else {
        walk = ctx->format_down;
    }

    INSTANCE(ctx->pidfile);

    START_COLOR((running ? "color_good" : "color_bad"));

    char string_status[STRING_SIZE];
    snprintf(string_status, STRING_SIZE, "%s", (running ? "yes" : "no"));

    placeholder_t placeholders[] = {
        {.name = "%title", .value = ctx->title},
        {.name = "%status", .value = string_status}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    char *formatted = format_placeholders(walk, &placeholders[0], num);
    OUTPUT_FORMATTED;
    END_COLOR;
    OUTPUT_FULL_TEXT(ctx->buf);
}
