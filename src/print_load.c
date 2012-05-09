// vim:ts=8:expandtab
#include "i3status.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

void print_load(yajl_gen json_gen, char *buffer, const char *format) {
        char *outwalk = buffer;
        /* Get load */

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(linux) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(sun)
        double loadavg[3];
        const char *walk;

        if (getloadavg(loadavg, 3) == -1)
                goto error;

        for (walk = format; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        *(outwalk++) = *walk;
                        continue;
                }

                if (BEGINS_WITH(walk+1, "1min")) {
                        outwalk += sprintf(outwalk, "%1.2f", loadavg[0]);
                        walk += strlen("1min");
                }

                if (BEGINS_WITH(walk+1, "5min")) {
                        outwalk += sprintf(outwalk, "%1.2f", loadavg[1]);
                        walk += strlen("5min");
                }

                if (BEGINS_WITH(walk+1, "15min")) {
                        outwalk += sprintf(outwalk, "%1.2f", loadavg[2]);
                        walk += strlen("15min");
                }
        }

        *outwalk = '\0';
        OUTPUT_FULL_TEXT(buffer);

        return;
error:
#endif
        OUTPUT_FULL_TEXT("cant read load");
        (void)fputs("i3status: Cannot read system load using getloadavg()\n", stderr);
}
