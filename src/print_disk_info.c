// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__APPLE__)
#include <sys/param.h>
#include <sys/mount.h>
#elif defined(__NetBSD__)
#else
#include <mntent.h>
#endif
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#define STRING_SIZE 20

#define BINARY_BASE UINT64_C(1024)
#define DECIMAL_BASE UINT64_C(1000)

#define MAX_EXPONENT 4

static const char *const iec_symbols[MAX_EXPONENT + 1] = {"", "Ki", "Mi", "Gi", "Ti"};
static const char *const si_symbols[MAX_EXPONENT + 1] = {"", "k", "M", "G", "T"};
static const char *const custom_symbols[MAX_EXPONENT + 1] = {"", "K", "M", "G", "T"};

/*
 * Formats bytes according to the given base and set of symbols.
 *
 */
static int format_bytes(char *outwalk, uint64_t bytes, uint64_t base, const char *const symbols[]) {
    double size = bytes;
    int exponent = 0;
    while (size >= base && exponent < MAX_EXPONENT) {
        size /= base;
        exponent += 1;
    }
    return sprintf(outwalk, "%.1f %sB", size, symbols[exponent]);
}

/*
 * Prints the given amount of bytes in a human readable manner.
 *
 */
static int print_bytes_human(char *outwalk, uint64_t bytes, const char *prefix_type) {
    if (strcasecmp(prefix_type, "decimal") == 0) {
        return format_bytes(outwalk, bytes, DECIMAL_BASE, si_symbols);
    } else if (strcasecmp(prefix_type, "custom") == 0) {
        return format_bytes(outwalk, bytes, BINARY_BASE, custom_symbols);
    } else {
        return format_bytes(outwalk, bytes, BINARY_BASE, iec_symbols);
    }
}

/*
 * Determines whether remaining bytes are below given threshold.
 *
 */
#if defined(__DragonFly__) || defined(__APPLE__)
static bool below_threshold(struct statfs buf, const char *prefix_type, const char *threshold_type, const double low_threshold) {
#else
static bool below_threshold(struct statvfs buf, const char *prefix_type, const char *threshold_type, const double low_threshold) {
#endif
    if (strcasecmp(threshold_type, "percentage_free") == 0) {
        return 100.0 * (double)buf.f_bfree / (double)buf.f_blocks < low_threshold;
    } else if (strcasecmp(threshold_type, "percentage_avail") == 0) {
        return 100.0 * (double)buf.f_bavail / (double)buf.f_blocks < low_threshold;
    } else if (strcasecmp(threshold_type, "bytes_free") == 0) {
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__APPLE__)
        return (double)buf.f_bsize * (double)buf.f_bfree < low_threshold;
#else
        return (double)buf.f_frsize * (double)buf.f_bfree < low_threshold;
#endif
    } else if (strcasecmp(threshold_type, "bytes_avail") == 0) {
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__APPLE__)
        return (double)buf.f_bsize * (double)buf.f_bavail < low_threshold;
#else
        return (double)buf.f_frsize * (double)buf.f_bavail < low_threshold;
#endif
    } else if (threshold_type[0] != '\0' && strncasecmp(threshold_type + 1, "bytes_", strlen("bytes_")) == 0) {
        uint64_t base = strcasecmp(prefix_type, "decimal") == 0 ? DECIMAL_BASE : BINARY_BASE;
        double factor = 1;

        switch (threshold_type[0]) {
            case 'T':
            case 't':
                factor *= base;
                /* fall-through */
            case 'G':
            case 'g':
                factor *= base;
                /* fall-through */
            case 'M':
            case 'm':
                factor *= base;
                /* fall-through */
            case 'K':
            case 'k':
                factor *= base;
                break;
            default:
                return false;
        }

        if (strcasecmp(threshold_type + 1, "bytes_free") == 0) {
            return (double)buf.f_bsize * (double)buf.f_bfree < low_threshold * factor;
        } else if (strcasecmp(threshold_type + 1, "bytes_avail") == 0) {
            return (double)buf.f_bsize * (double)buf.f_bavail < low_threshold * factor;
        }
    }

    return false;
}

/*
 * Does a statvfs and prints either free, used or total amounts of bytes in a
 * human readable manner.
 *
 */
void print_disk_info(disk_info_ctx_t *ctx) {
    const char *selected_format = ctx->format;
    char *outwalk = ctx->buf;
    bool colorful_output = false;
    bool mounted = false;

    INSTANCE(ctx->path);

#if defined(__DragonFly__) || defined(__APPLE__)
    struct statfs buf;

    if (statfs(path, &buf) == -1)
        return;

    mounted = true;
#elif defined(__NetBSD__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__)
    struct statvfs buf;

    if (statvfs(ctx->path, &buf) == -1)
        return;

    mounted = true;
#else
    struct statvfs buf;

    if (statvfs(ctx->path, &buf) == -1) {
        /* If statvfs errors, e.g., due to the path not existing,
         * we consider the device not mounted. */
        mounted = false;
    } else {
        char *sanitized = sstrdup(ctx->path);
        if (strlen(sanitized) > 1 && sanitized[strlen(sanitized) - 1] == '/')
            sanitized[strlen(sanitized) - 1] = '\0';
        FILE *mntentfile = setmntent("/etc/mtab", "r");
        if (mntentfile == NULL) {
            mntentfile = setmntent("/proc/mounts", "r");
        }
        if (mntentfile == NULL) {
            fprintf(stderr, "i3status: files /etc/mtab and /proc/mounts aren't accessible\n");
        } else {
            struct mntent *m;

            while ((m = getmntent(mntentfile)) != NULL) {
                if (strcmp(m->mnt_dir, sanitized) == 0) {
                    mounted = true;
                    break;
                }
            }
            endmntent(mntentfile);
        }
        free(sanitized);
    }
#endif

    if (!mounted) {
        if (ctx->format_not_mounted == NULL)
            ctx->format_not_mounted = "";
        selected_format = ctx->format_not_mounted;
    } else if (ctx->low_threshold > 0 && below_threshold(buf, ctx->prefix_type, ctx->threshold_type, ctx->low_threshold)) {
        START_COLOR("color_bad");
        colorful_output = true;
        if (ctx->format_below_threshold != NULL)
            selected_format = ctx->format_below_threshold;
    }

    char string_free[STRING_SIZE];
    char string_used[STRING_SIZE];
    char string_total[STRING_SIZE];
    char string_avail[STRING_SIZE];
    char string_percentage_free[STRING_SIZE];
    char string_percentage_used_of_avail[STRING_SIZE];
    char string_percentage_used[STRING_SIZE];
    char string_percentage_avail[STRING_SIZE];

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__APPLE__)
    print_bytes_human(string_free, (uint64_t)buf.f_bsize * (uint64_t)buf.f_bfree, ctx->prefix_type);
    print_bytes_human(string_used, (uint64_t)buf.f_bsize * ((uint64_t)buf.f_blocks - (uint64_t)buf.f_bfree), ctx->prefix_type);
    print_bytes_human(string_total, (uint64_t)buf.f_bsize * (uint64_t)buf.f_blocks, ctx->prefix_type);
    print_bytes_human(string_avail, (uint64_t)buf.f_bsize * (uint64_t)buf.f_bavail, ctx->prefix_type);
#else
    print_bytes_human(string_free, (uint64_t)buf.f_frsize * (uint64_t)buf.f_bfree, ctx->prefix_type);
    print_bytes_human(string_used, (uint64_t)buf.f_frsize * ((uint64_t)buf.f_blocks - (uint64_t)buf.f_bfree), ctx->prefix_type);
    print_bytes_human(string_total, (uint64_t)buf.f_frsize * (uint64_t)buf.f_blocks, ctx->prefix_type);
    print_bytes_human(string_avail, (uint64_t)buf.f_frsize * (uint64_t)buf.f_bavail, ctx->prefix_type);
#endif
    snprintf(string_percentage_free, STRING_SIZE, "%.01f%s", 100.0 * (double)buf.f_bfree / (double)buf.f_blocks, pct_mark);
    snprintf(string_percentage_used_of_avail, STRING_SIZE, "%.01f%s", 100.0 * (double)(buf.f_blocks - buf.f_bavail) / (double)buf.f_blocks, pct_mark);
    snprintf(string_percentage_used, STRING_SIZE, "%.01f%s", 100.0 * (double)(buf.f_blocks - buf.f_bfree) / (double)buf.f_blocks, pct_mark);
    snprintf(string_percentage_avail, STRING_SIZE, "%.01f%s", 100.0 * (double)buf.f_bavail / (double)buf.f_blocks, pct_mark);

    placeholder_t placeholders[] = {
        {.name = "%free", .value = string_free},
        {.name = "%used", .value = string_used},
        {.name = "%total", .value = string_total},
        {.name = "%avail", .value = string_avail},
        {.name = "%percentage_free", .value = string_percentage_free},
        {.name = "%percentage_used_of_avail", .value = string_percentage_used_of_avail},
        {.name = "%percentage_used", .value = string_percentage_used},
        {.name = "%percentage_avail", .value = string_percentage_avail}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    char *formatted = format_placeholders(selected_format, &placeholders[0], num);
    OUTPUT_FORMATTED;
    free(formatted);

    if (colorful_output)
        END_COLOR;

    *outwalk = '\0';
    OUTPUT_FULL_TEXT(ctx->buf);
}
