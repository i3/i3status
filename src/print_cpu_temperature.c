// vim:ts=8:expandtab
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "i3status.h"

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <err.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#define TZ_ZEROC 2732
#define TZ_KELVTOC(x) (((x) - TZ_ZEROC) / 10), abs(((x) - TZ_ZEROC) % 10)
#endif

static char *thermal_zone;

/*
 * Reads the CPU temperature from /sys/class/thermal/thermal_zone0/temp and
 * returns the temperature in degree celcius.
 *
 */
void print_cpu_temperature_info(int zone, const char *path, const char *format) {
#ifdef THERMAL_ZONE
        const char *walk;
        static char buf[16];

        if (path == NULL) {
                asprintf(&thermal_zone, THERMAL_ZONE, zone);
                path = thermal_zone;
        }

        for (walk = format; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        putchar(*walk);
                        continue;
                }

                if (BEGINS_WITH(walk+1, "degrees")) {
#if defined(LINUX)
                        long int temp;
                        if (!slurp(path, buf, sizeof(buf)))
                                die("Could not open \"%s\"\n", path);
                        temp = strtol(buf, NULL, 10);
                        if (temp == LONG_MIN || temp == LONG_MAX || temp <= 0)
                                (void)printf("?");
                        else
                                (void)printf("%ld", (temp/1000));
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
                        int sysctl_rslt;
                        size_t sysctl_size = sizeof(sysctl_rslt);
                        if (sysctlbyname(path, &sysctl_rslt, &sysctl_size, NULL, 0)) {
                                (void)printf("No thermal zone found");
                                return;
                        }

                        (void)printf("%d.%d", TZ_KELVTOC(sysctl_rslt));
#endif
                        walk += strlen("degrees");
                }
        }
#endif
}
