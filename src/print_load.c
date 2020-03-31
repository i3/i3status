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

void print_load(yajl_gen json_gen, char *buffer, const char *format, const char *format_above_threshold, const float max_threshold) {
    char *outwalk = buffer;
/* Get load */

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(linux) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(sun) || defined(__DragonFly__)
    double loadavg[3];
    const char *selected_format = format;
    bool colorful_output = false;

    if (getloadavg(loadavg, 3) == -1)
        goto error;

    if (loadavg[0] >= max_threshold) {
        START_COLOR("color_bad");
        colorful_output = true;
        if (format_above_threshold != NULL)
            selected_format = format_above_threshold;
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
    buffer = format_placeholders(selected_format, &placeholders[0], num);

    if (colorful_output)
        END_COLOR;

    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);
    free(buffer);

    return;
error:
#endif
    OUTPUT_FULL_TEXT("cant read load");
    (void)fputs("i3status: Cannot read system load using getloadavg()\n", stderr);
}
