// vim:sw=8:sts=8:ts=8:expandtab
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/dkstat.h>
#endif

#include "i3status.h"

static int prev_total = 0;
static int prev_idle  = 0;

/*
 * Reads the CPU utilization from /proc/stat and returns the usage as a
 * percentage.
 *
 */
void print_cpu_usage(const char *format) {
        const char *walk;
        char buf[1024];
        int curr_user = 0, curr_nice = 0, curr_system = 0, curr_idle = 0, curr_total;
        int diff_idle, diff_total, diff_usage;

#if defined(LINUX)
        static char statpath[512];
        strcpy(statpath, "/proc/stat");
        if (!slurp(statpath, buf, sizeof(buf) ||
            sscanf(buf, "cpu %d %d %d %d", &curr_user, &curr_nice, &curr_system, &curr_idle) != 4))
                goto error;

        curr_total = curr_user + curr_nice + curr_system + curr_idle;
        diff_idle  = curr_idle - prev_idle;
        diff_total = curr_total - prev_total;
        diff_usage = (1000 * (diff_total - diff_idle)/diff_total + 5)/10;
        prev_total = curr_total;
        prev_idle  = curr_idle;
#elif defined(__FreeBSD__)
        size_t size;
        long cp_time[CPUSTATES];
        size = sizeof cp_time;
        if (sysctlbyname("kern.cp_time", &cp_time, &size, NULL, 0) < 0)
                goto error;

        curr_user = cp_time[CP_USER];
        curr_nice = cp_time[CP_NICE];
        curr_system = cp_time[CP_SYS];
        curr_idle = cp_time[CP_IDLE];
        curr_total = curr_user + curr_nice + curr_system + curr_idle;
        diff_idle  = curr_idle - prev_idle;
        diff_total = curr_total - prev_total;
        diff_usage = (1000 * (diff_total - diff_idle)/diff_total + 5)/10;
        prev_total = curr_total;
        prev_idle  = curr_idle;
#else
        goto error;
#endif
        for (walk = format; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        putchar(*walk);
                        continue;
                }

                if (strncmp(walk+1, "usage", strlen("usage")) == 0) {
                        printf("%02d%%", diff_usage);
                        walk += strlen("usage");
                }
        }
        return;
error:
        (void)fputs("Cannot read usage\n", stderr);
}
