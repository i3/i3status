// vim:ts=8:expandtab
#include "i3status.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void print_load(const char *format) {
/* Get load */
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(linux) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(sun)
        double loadavg[3];
        const char *walk;

        if (getloadavg(loadavg, 3) == -1)
                goto error;

        for (walk = format; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        putchar(*walk);
                        continue;
                }

                if (BEGINS_WITH(walk+1, "1min")) {
                        (void)printf("%1.2f", loadavg[0]);
                        walk += strlen("1min");
                }

                if (BEGINS_WITH(walk+1, "5min")) {
                        (void)printf("%1.2f", loadavg[1]);
                        walk += strlen("5min");
                }

                if (BEGINS_WITH(walk+1, "15min")) {
                        (void)printf("%1.2f", loadavg[2]);
                        walk += strlen("15min");
                }
        }
        return;
error:
#endif
        (void)fputs("Cannot read load\n", stderr);
}
