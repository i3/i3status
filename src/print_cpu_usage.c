// vim:ts=4:sw=4:expandtab
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#if defined(__OpenBSD__)
#include <sys/sched.h>
#else
#include <sys/dkstat.h>
#endif
#endif

#if defined(__DragonFly__)
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#endif

#if defined(__NetBSD__)
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/sched.h>
#endif

#include "i3status.h"

static int prev_total = 0;
static int prev_idle = 0;

/*
 * Reads the CPU utilization from /proc/stat and returns the usage as a
 * percentage.
 *
 */
void print_cpu_usage(yajl_gen json_gen, char *buffer, const char *format) {
    const char *walk;
    char *outwalk = buffer;
    int curr_user = 0, curr_nice = 0, curr_system = 0, curr_idle = 0, curr_total;
    int diff_idle, diff_total, diff_usage;

#if defined(LINUX)
    static char statpath[512];
    char buf[1024];
    strcpy(statpath, "/proc/stat");
    if (!slurp(statpath, buf, sizeof(buf)) ||
        sscanf(buf, "cpu %d %d %d %d", &curr_user, &curr_nice, &curr_system, &curr_idle) != 4)
        goto error;

    curr_total = curr_user + curr_nice + curr_system + curr_idle;
    diff_idle = curr_idle - prev_idle;
    diff_total = curr_total - prev_total;
    diff_usage = (diff_total ? (1000 * (diff_total - diff_idle) / diff_total + 5) / 10 : 0);
    prev_total = curr_total;
    prev_idle = curr_idle;
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)
    size_t size;
    long cp_time[CPUSTATES];
    size = sizeof cp_time;
    if (sysctlbyname("kern.cp_time", &cp_time, &size, NULL, 0) < 0)
        goto error;
#else
    /* This information is taken from the boot cpu, any other cpus are currently ignored. */
    long cp_time[CPUSTATES];
    int mib[2];
    size_t size = sizeof(cp_time);

    mib[0] = CTL_KERN;
    mib[1] = KERN_CPTIME;

    if (sysctl(mib, 2, cp_time, &size, NULL, 0))
        goto error;
#endif

    curr_user = cp_time[CP_USER];
    curr_nice = cp_time[CP_NICE];
    curr_system = cp_time[CP_SYS];
    curr_idle = cp_time[CP_IDLE];
    curr_total = curr_user + curr_nice + curr_system + curr_idle;
    diff_idle = curr_idle - prev_idle;
    diff_total = curr_total - prev_total;
    diff_usage = (diff_total ? (1000 * (diff_total - diff_idle) / diff_total + 5) / 10 : 0);
    prev_total = curr_total;
    prev_idle = curr_idle;
#else
    goto error;
#endif
    for (walk = format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "usage")) {
            outwalk += sprintf(outwalk, "%02d%s", diff_usage, pct_mark);
            walk += strlen("usage");
        }
    }

    OUTPUT_FULL_TEXT(buffer);
    return;
error:
    OUTPUT_FULL_TEXT("cant read cpu usage");
    (void)fputs("i3status: Cannot read CPU usage\n", stderr);
}
