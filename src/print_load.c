// vim:ts=4:sw=4:expandtab
#include "i3status.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

void print_load(yajl_gen json_gen, char *buffer, const char *format, const float max_threshold) {
    char *outwalk = buffer;
/* Get load */

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(linux) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(sun) || defined(__DragonFly__)
    double loadavg[3];
    const char *walk;
    bool colorful_output = false;

    if (getloadavg(loadavg, 3) == -1)
        goto error;

    for (walk = format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }
        if (loadavg[0] >= max_threshold) {
            START_COLOR("color_bad");
            colorful_output = true;
        }

        if (BEGINS_WITH(walk + 1, "1min")) {
            outwalk += sprintf(outwalk, "%1.2f", loadavg[0]);
            walk += strlen("1min");
        }

        if (BEGINS_WITH(walk + 1, "5min")) {
            outwalk += sprintf(outwalk, "%1.2f", loadavg[1]);
            walk += strlen("5min");
        }

        if (BEGINS_WITH(walk + 1, "15min")) {
            outwalk += sprintf(outwalk, "%1.2f", loadavg[2]);
            walk += strlen("15min");
        }
        if (colorful_output)
            END_COLOR;
    }

    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);

    return;
error:
#endif
    OUTPUT_FULL_TEXT("cant read load");
    (void)fputs("i3status: Cannot read system load using getloadavg()\n", stderr);
}
