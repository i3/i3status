// vim:ts=4:sw=4:expandtab
#include <config.h>
#if defined(__linux__)
#include <sys/sysinfo.h>
#endif
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

#define STRING_SIZE 10

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
static placeholder_t *placeholders = NULL;

/*
 * Reads the CPU utilization from /proc/stat and returns the usage as a
 * percentage.
 *
 */
void print_cpu_usage(yajl_gen json_gen, char *buffer, const char *format, const char *format_above_threshold, const char *format_above_degraded_threshold, const char *path, const float max_threshold, const float degraded_threshold) {
    const char *selected_format = format;
    char *outwalk = buffer;
    struct cpu_usage curr_all = {0, 0, 0, 0, 0};
    int diff_idle, diff_total, diff_usage;
    bool colorful_output = false;

#if defined(__linux__)

    // Detecting if CPU count has changed
    int curr_cpu_count = get_nprocs_conf();
    if (curr_cpu_count != cpu_count) {
        cpu_count = curr_cpu_count;

        free(prev_cpus);
        free(curr_cpus);
        free(placeholders);

        prev_cpus = calloc(cpu_count, sizeof(struct cpu_usage));
        curr_cpus = calloc(cpu_count, sizeof(struct cpu_usage));
        placeholders = calloc(cpu_count + 1, sizeof(placeholder_t));
    }

    memcpy(curr_cpus, prev_cpus, cpu_count * sizeof(struct cpu_usage));
    char buf[4096];
    curr_cpu_count = get_nprocs();
    if (!slurp(path, buf, sizeof(buf)))
        goto error;
    // Parsing all cpu values using strtok
    if (strtok(buf, "\n") == NULL)
        goto error;
    char *buf_itr = NULL;
    for (int idx = 0; idx < curr_cpu_count; ++idx) {
        buf_itr = strtok(NULL, "\n");
        int cpu_idx, user, nice, system, idle;
        if (!buf_itr || sscanf(buf_itr, "cpu%d %d %d %d %d", &cpu_idx, &user, &nice, &system, &idle) != 5) {
            goto error;
        }
        if (cpu_idx < 0 || cpu_idx >= cpu_count)
            goto error;
        curr_cpus[cpu_idx].user = user;
        curr_cpus[cpu_idx].nice = nice;
        curr_cpus[cpu_idx].system = system;
        curr_cpus[cpu_idx].idle = idle;
        curr_cpus[cpu_idx].total = user + nice + system + idle;
    }
    for (int cpu_idx = 0; cpu_idx < cpu_count; cpu_idx++) {
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

    char string_usage[STRING_SIZE];

    sprintf(string_usage, "%02d%s", diff_usage, pct_mark);

#if defined(__linux__)
    for (int number = 0; number < cpu_count; number++) {
        int cpu_diff_idle = curr_cpus[number].idle - prev_cpus[number].idle;
        int cpu_diff_total = curr_cpus[number].total - prev_cpus[number].total;
        int cpu_diff_usage = (cpu_diff_total ? (1000 * (cpu_diff_total - cpu_diff_idle) / cpu_diff_total + 5) / 10 : 0);
        // replace with placeholder_t.name an placeholder_t.value
        placeholders[number].name = malloc(sizeof(char) * STRING_SIZE);
        placeholders[number].value = malloc(sizeof(char) * STRING_SIZE);
        sprintf(placeholders[number].name, "%%cpu%d", number);
        sprintf(placeholders[number].value, "%02d%s", cpu_diff_usage, pct_mark);
    }
#endif

    // overall usage
    placeholders[cpu_count].name = "%usage";
    placeholders[cpu_count].value = string_usage;

    const size_t num = cpu_count + 1;
    buffer = format_placeholders(selected_format, &placeholders[0], num);

    // free everything
    for (int i = 0; i < cpu_count; i++) {
        free(placeholders[i].name);
        free(placeholders[i].value);
    }

    struct cpu_usage *temp_cpus = prev_cpus;
    prev_cpus = curr_cpus;
    curr_cpus = temp_cpus;

    if (colorful_output)
        END_COLOR;

    OUTPUT_FULL_TEXT(buffer);
    return;
error:
    OUTPUT_FULL_TEXT("cant read cpu usage");
    (void)fputs("i3status: Cannot read CPU usage\n", stderr);
}
