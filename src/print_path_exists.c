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

void print_path_exists(path_exists_ctx_t *ctx) {
    const char *walk;
    char *outwalk = ctx->buf;
    output_color_t outcolor = COLOR_DEFAULT;
    struct stat st;
    const bool exists = (stat(ctx->path, &st) == 0);

    if (exists || ctx->format_down == NULL) {
        walk = ctx->format;
    } else {
        walk = ctx->format_down;
    }

    INSTANCE(ctx->path);

    outcolor = (exists ? COLOR_GOOD : COLOR_BAD);

    char string_status[STRING_SIZE];

    snprintf(string_status, STRING_SIZE, "%s", (exists ? "yes" : "no"));

    placeholder_t placeholders[] = {
        {.name = "%title", .value = ctx->title},
        {.name = "%status", .value = string_status}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    char *formatted = format_placeholders(walk, &placeholders[0], num);
    OUTPUT_FORMATTED;
    free(formatted);

    OUTPUT_FULL_TEXT(ctx->buf);
}
