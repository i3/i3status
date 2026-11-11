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
#include <errno.h>

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
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    long user;
    long nice;
    long system;
    long idle;
    long total;
#else
    int user;
    int nice;
    int system;
    int idle;
#if defined(__OpenBSD__)
    int spin;
#endif
    int total;
#endif
};

#if defined(__linux__)
static int cpu_count = 0;
#endif
static struct cpu_usage prev_all = {0, 0, 0, 0, 0};
static struct cpu_usage *prev_cpus = NULL;
static struct cpu_usage *curr_cpus = NULL;

/*
 * Returns the name of the color option to use for the given usage percentage,
 * or NULL if the usage is below both thresholds.
 *
 */
static const char *usage_color(const cpu_usage_ctx_t *ctx, int usage) {
    if (usage >= ctx->max_threshold)
        return "color_bad";
    if (usage >= ctx->degraded_threshold)
        return "color_degraded";
    return NULL;
}

/*
 * Whether a single value can be colored within the output. i3bar can only
 * color a whole block, unless pango markup is enabled.
 *
 */
static bool has_inline_colors(void) {
    return output_format != O_I3BAR || markup_format == M_PANGO;
}

/*
 * Writes the given usage percentage to outwalk, colored according to the
 * thresholds if the output format supports coloring a single value. Returns
 * the position after the written text.
 *
 */
static char *print_usage(const cpu_usage_ctx_t *ctx, char *outwalk, int usage) {
    char start[64] = "";
    const char *end = "";
    const char *colorstr = usage_color(ctx, usage);

    if (colorstr != NULL && has_inline_colors() && cfg_getbool(cfg_general, "colors")) {
        if (output_format == O_I3BAR) {
            const char *val = NULL;
            if (cfg_section)
                val = cfg_getstr(cfg_section, colorstr);
            if (!val)
                val = cfg_getstr(cfg_general, colorstr);
            (void)snprintf(start, sizeof(start), "<span foreground='%s'>", val);
            end = "</span>";
        } else {
            (void)snprintf(start, sizeof(start), "%s", color(colorstr));
            /* endcolor() does not reset the color for dzen2 and lemonbar,
             * which would color everything up to the next separator. */
            if (output_format == O_DZEN2)
                end = "^fg()";
            else if (output_format == O_LEMONBAR)
                end = "%{F-}";
            else
                end = endcolor();
        }
    }

    /* Colors make a value a lot longer than its visible width, so make sure
     * that many colored values cannot overflow the buffer. A value which does
     * not fit is dropped entirely to not leave unbalanced markup behind. */
    const size_t remaining = ctx->buf + ctx->buflen - outwalk;
    const int written = snprintf(outwalk, remaining, "%s%02d%s%s", start, usage, pct_mark, end);
    if (written < 0 || (size_t)written >= remaining) {
        *outwalk = '\0';
        return outwalk;
    }
    return outwalk + written;
}

/*
 * Reads the CPU utilization from /proc/stat and returns the usage as a
 * percentage.
 *
 */
void print_cpu_usage(cpu_usage_ctx_t *ctx) {
    const char *selected_format = ctx->format;
    const char *walk;
    char *outwalk = ctx->buf;
    struct cpu_usage curr_all = {0, 0, 0, 0, 0};
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    long diff_idle, diff_total;
#else
    int diff_idle, diff_total;
#endif
    int diff_usage;

#if defined(__linux__)

    // Detecting if CPU count has changed
    int curr_cpu_count = get_nprocs_conf();
    if (curr_cpu_count != cpu_count) {
        cpu_count = curr_cpu_count;
        free(prev_cpus);
        prev_cpus = (struct cpu_usage *)calloc(cpu_count, sizeof(struct cpu_usage));
        free(curr_cpus);
        curr_cpus = (struct cpu_usage *)calloc(cpu_count, sizeof(struct cpu_usage));
    }

    memcpy(curr_cpus, prev_cpus, cpu_count * sizeof(struct cpu_usage));
    FILE *f = fopen(ctx->path, "r");
    if (f == NULL) {
        fprintf(stderr, "i3status: open %s: %s\n", ctx->path, strerror(errno));
        goto error;
    }
    curr_cpu_count = get_nprocs();
    char line[4096];

    /* Discard first line (cpu ), start at second line (cpu0) */
    if (fgets(line, sizeof(line), f) == NULL) {
        fclose(f);
        goto error; /* unexpected EOF or read error */
    }

    for (int idx = 0; idx < curr_cpu_count; ++idx) {
        if (fgets(line, sizeof(line), f) == NULL) {
            fclose(f);
            goto error; /* unexpected EOF or read error */
        }
        int cpu_idx, user, nice, system, idle;
        if (sscanf(line, "cpu%d %d %d %d %d", &cpu_idx, &user, &nice, &system, &idle) != 5) {
            fclose(f);
            goto error;
        }
        if (cpu_idx < 0 || cpu_idx >= cpu_count) {
            fclose(f);
            goto error;
        }
        curr_cpus[cpu_idx].user = user;
        curr_cpus[cpu_idx].nice = nice;
        curr_cpus[cpu_idx].system = system;
        curr_cpus[cpu_idx].idle = idle;
        curr_cpus[cpu_idx].total = user + nice + system + idle;
    }
    fclose(f);
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
#if defined(__DragonFly__)
    curr_all.system += cp_time[CP_INTR];
#endif
    curr_all.idle = cp_time[CP_IDLE];
#if defined(__OpenBSD__)
    curr_all.spin = cp_time[CP_SPIN];
    curr_all.total = curr_all.user + curr_all.nice + curr_all.system + curr_all.idle + curr_all.spin;
#else
    curr_all.total = curr_all.user + curr_all.nice + curr_all.system + curr_all.idle;
#endif
    diff_idle = curr_all.idle - prev_all.idle;
    diff_total = curr_all.total - prev_all.total;
    diff_usage = (diff_total ? (1000 * (diff_total - diff_idle) / diff_total + 5) / 10 : 0);
    prev_all = curr_all;
#else
    goto error;
#endif

    /* Every value is colored on its own. Where that is not possible, color
     * the whole block based on the overall usage instead. */
    if (!has_inline_colors()) {
        const char *colorstr = usage_color(ctx, diff_usage);
        if (colorstr != NULL)
            START_COLOR(colorstr);
    }

    if (diff_usage >= ctx->max_threshold) {
        if (ctx->format_above_threshold != NULL)
            selected_format = ctx->format_above_threshold;
    } else if (diff_usage >= ctx->degraded_threshold) {
        if (ctx->format_above_degraded_threshold != NULL)
            selected_format = ctx->format_above_degraded_threshold;
    }

    for (walk = selected_format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;

        } else if (BEGINS_WITH(walk + 1, "usage")) {
            outwalk = print_usage(ctx, outwalk, diff_usage);
            walk += strlen("usage");
        }
#if defined(__linux__)
        else if (BEGINS_WITH(walk + 1, "cpu")) {
            int number = -1;
            int length = strlen("cpu");
            sscanf(walk + 1, "cpu%d%n", &number, &length);
            if (number == -1) {
                fprintf(stderr, "i3status: provided CPU number cannot be parsed\n");
            } else if (number >= cpu_count) {
                fprintf(stderr, "i3status: provided CPU number '%d' above detected number of CPU %d\n", number, cpu_count);
            } else {
                int cpu_diff_idle = curr_cpus[number].idle - prev_cpus[number].idle;
                int cpu_diff_total = curr_cpus[number].total - prev_cpus[number].total;
                int cpu_diff_usage = (cpu_diff_total ? (1000 * (cpu_diff_total - cpu_diff_idle) / cpu_diff_total + 5) / 10 : 0);
                outwalk = print_usage(ctx, outwalk, cpu_diff_usage);
            }
            walk += length;
        }
#endif
        else {
            *(outwalk++) = '%';
        }
    }

    struct cpu_usage *temp_cpus = prev_cpus;
    prev_cpus = curr_cpus;
    curr_cpus = temp_cpus;

    OUTPUT_FULL_TEXT(ctx->buf);
    return;
error:
    OUTPUT_FULL_TEXT("cant read cpu usage");
    (void)fputs("i3status: Cannot read CPU usage\n", stderr);
}
