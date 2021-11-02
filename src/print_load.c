// vim:ts=4:sw=4:expandtab
#include <config.h>
#include "i3status.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#define STRING_SIZE 10

void print_load(load_ctx_t *ctx) {
    char *outwalk = ctx->buf;
    /* Get load */

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__linux__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(sun) || defined(__DragonFly__)
    double loadavg[3];
    const char *selected_format = ctx->format;
    bool colorful_output = false;

    if (getloadavg(loadavg, 3) == -1)
        goto error;

    if (loadavg[0] >= ctx->max_threshold) {
        START_COLOR("color_bad");
        colorful_output = true;
        if (ctx->format_above_threshold != NULL)
            selected_format = ctx->format_above_threshold;
    }

    char string_loadavg_1[STRING_SIZE];
    char string_loadavg_5[STRING_SIZE];
    char string_loadavg_15[STRING_SIZE];

    snprintf(string_loadavg_1, STRING_SIZE, "%1.2f", loadavg[0]);
    snprintf(string_loadavg_5, STRING_SIZE, "%1.2f", loadavg[1]);
    snprintf(string_loadavg_15, STRING_SIZE, "%1.2f", loadavg[2]);

    placeholder_t placeholders[] = {
        {.name = "%1min", .value = string_loadavg_1},
        {.name = "%5min", .value = string_loadavg_5},
        {.name = "%15min", .value = string_loadavg_15}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    char *formatted = format_placeholders(selected_format, &placeholders[0], num);
    OUTPUT_FORMATTED;
    free(formatted);

    if (colorful_output)
        END_COLOR;

    *outwalk = '\0';
    OUTPUT_FULL_TEXT(ctx->buf);

    return;
error:
#endif
    OUTPUT_FULL_TEXT("cant read load");
    (void)fputs("i3status: Cannot read system load using getloadavg()\n", stderr);
}
