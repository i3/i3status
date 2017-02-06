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

struct cpu_usage {
    int user;
    int nice;
    int system;
    int idle;
    int total;
};

static int cpu_count = 0;
static struct cpu_usage prev_all = {0, 0, 0, 0, 0};
static struct cpu_usage *prev_cpus = NULL;
static struct cpu_usage *curr_cpus = NULL;

/*
 * Reads the CPU utilization from /proc/stat and returns the usage as a
 * percentage.
 *
 */
void print_cpu_usage(yajl_gen json_gen, char *buffer, const char *format, const char *format_above_threshold, const char *format_above_degraded_threshold, const char *path, const float max_threshold, const float degraded_threshold) {
    const char *selected_format = format;
    const char *walk;
    char *outwalk = buffer;
    struct cpu_usage curr_all = {0, 0, 0, 0, 0};
    int diff_idle, diff_total, diff_usage;
    bool colorful_output = false;

#if defined(LINUX)

    // Detecting if CPU count has changed
    int curr_cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (curr_cpu_count != cpu_count) {
        cpu_count = curr_cpu_count;
        free(prev_cpus);
        prev_cpus = (struct cpu_usage *)calloc(cpu_count, sizeof(struct cpu_usage));
        free(curr_cpus);
        curr_cpus = (struct cpu_usage *)calloc(cpu_count, sizeof(struct cpu_usage));
    }

    char buf[4096];
    if (!slurp(path, buf, sizeof(buf)))
        goto error;
    // Parsing all cpu values using strtok
    if (strtok(buf, "\n") == NULL)
        goto error;
    char *buf_itr = NULL;
    for (int cpu_idx = 0; cpu_idx < cpu_count; cpu_idx++) {
        buf_itr = strtok(NULL, "\n");
        int curr_cpu_idx = -1;
        if (!buf_itr || sscanf(buf_itr, "cpu%d %d %d %d %d", &curr_cpu_idx, &curr_cpus[cpu_idx].user, &curr_cpus[cpu_idx].nice, &curr_cpus[cpu_idx].system, &curr_cpus[cpu_idx].idle) != 5 || curr_cpu_idx != cpu_idx)
            goto error;
        curr_cpus[cpu_idx].total = curr_cpus[cpu_idx].user + curr_cpus[cpu_idx].nice + curr_cpus[cpu_idx].system + curr_cpus[cpu_idx].idle;
        curr_all.user += curr_cpus[cpu_idx].user;
        curr_all.nice += curr_cpus[cpu_idx].nice;
        curr_all.system += curr_cpus[cpu_idx].system;
        curr_all.idle += curr_cpus[cpu_idx].idle;
        curr_all.total += curr_cpus[cpu_idx].total;
    }

    diff_idle = curr_all.idle - prev_all.idle;
    diff_total = curr_all.total - prev_all.total;
    diff_usage = (diff_total ? (1000 * (diff_total - diff_idle) / diff_total + 5) / 10 : 0);
    prev_all = curr_all;
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

    curr_all.user = cp_time[CP_USER];
    curr_all.nice = cp_time[CP_NICE];
    curr_all.system = cp_time[CP_SYS];
    curr_all.idle = cp_time[CP_IDLE];
    curr_all.total = curr_all.user + curr_all.nice + curr_all.system + curr_all.idle;
    diff_idle = curr_all.idle - prev_all.idle;
    diff_total = curr_all.total - prev_all.total;
    diff_usage = (diff_total ? (1000 * (diff_total - diff_idle) / diff_total + 5) / 10 : 0);
    prev_all = curr_all;
#else
    goto error;
#endif

    if (diff_usage >= max_threshold) {
        START_COLOR("color_bad");
        colorful_output = true;
        if (format_above_threshold != NULL)
            selected_format = format_above_threshold;
    } else if (diff_usage >= degraded_threshold) {
        START_COLOR("color_degraded");
        colorful_output = true;
        if (format_above_degraded_threshold != NULL)
            selected_format = format_above_degraded_threshold;
    }

    for (walk = selected_format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "usage")) {
            outwalk += sprintf(outwalk, "%02d%s", diff_usage, pct_mark);
            walk += strlen("usage");
        }
#if defined(LINUX)
        if (BEGINS_WITH(walk + 1, "cpu")) {
            int number = 0;
            sscanf(walk + 1, "cpu%d", &number);
            if (number < 0 || number >= cpu_count) {
                fprintf(stderr, "provided CPU number '%d' above detected number of CPU %d\n", number, cpu_count);
            } else {
                int cpu_diff_idle = curr_cpus[number].idle - prev_cpus[number].idle;
                int cpu_diff_total = curr_cpus[number].total - prev_cpus[number].total;
                int cpu_diff_usage = (cpu_diff_total ? (1000 * (cpu_diff_total - cpu_diff_idle) / cpu_diff_total + 5) / 10 : 0);
                outwalk += sprintf(outwalk, "%02d%s", cpu_diff_usage, pct_mark);
            }
            int padding = 1;
            int step = 10;
            while (step < number) {
                step *= 10;
                padding++;
            }
            walk += strlen("cpu") + padding;
        }
#endif
    }

    for (int i = 0; i < cpu_count; i++)
        prev_cpus[i] = curr_cpus[i];

    if (colorful_output)
        END_COLOR;

    OUTPUT_FULL_TEXT(buffer);
    return;
error:
    OUTPUT_FULL_TEXT("cant read cpu usage");
    (void)fputs("i3status: Cannot read CPU usage\n", stderr);
}
