// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || (__OpenBSD__) || defined(__DragonFly__) || defined(__APPLE__)
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <mntent.h>
#endif
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

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
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__APPLE__)
static bool below_threshold(struct statfs buf, const char *prefix_type, const char *threshold_type, const double low_threshold) {
#else
static bool below_threshold(struct statvfs buf, const char *prefix_type, const char *threshold_type, const double low_threshold) {
#endif
    if (strcasecmp(threshold_type, "percentage_free") == 0) {
        return 100.0 * (double)buf.f_bfree / (double)buf.f_blocks < low_threshold;
    } else if (strcasecmp(threshold_type, "percentage_avail") == 0) {
        return 100.0 * (double)buf.f_bavail / (double)buf.f_blocks < low_threshold;
    } else if (strcasecmp(threshold_type, "bytes_free") == 0) {
        return (double)buf.f_bsize * (double)buf.f_bfree < low_threshold;
    } else if (strcasecmp(threshold_type, "bytes_avail") == 0) {
        return (double)buf.f_bsize * (double)buf.f_bavail < low_threshold;
    } else if (threshold_type[0] != '\0' && strncasecmp(threshold_type + 1, "bytes_", strlen("bytes_")) == 0) {
        uint64_t base = strcasecmp(prefix_type, "decimal") == 0 ? DECIMAL_BASE : BINARY_BASE;
        double factor = 1;

        switch (threshold_type[0]) {
            case 'T':
            case 't':
                factor *= base;
            case 'G':
            case 'g':
                factor *= base;
            case 'M':
            case 'm':
                factor *= base;
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
void print_disk_info(yajl_gen json_gen, char *buffer, const char *path, const char *format, const char *format_not_mounted, const char *prefix_type, const char *threshold_type, const double low_threshold) {
    const char *walk;
    char *outwalk = buffer;
    bool colorful_output = false;

    INSTANCE(path);

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__APPLE__)
    struct statfs buf;

    if (statfs(path, &buf) == -1)
        return;
#else
    struct statvfs buf;

    if (statvfs(path, &buf) == -1) {
        /* If statvfs errors, e.g., due to the path not existing,
         * we use the format for a not mounted device. */
        format = format_not_mounted;
    } else if (format_not_mounted != NULL) {
        FILE *mntentfile = setmntent("/etc/mtab", "r");
        struct mntent *m;
        bool found = false;

        while ((m = getmntent(mntentfile)) != NULL) {
            if (strcmp(m->mnt_dir, path) == 0) {
                found = true;
                break;
            }
        }
        endmntent(mntentfile);

        if (!found) {
            format = format_not_mounted;
        }
    }
#endif

    if (low_threshold > 0 && below_threshold(buf, prefix_type, threshold_type, low_threshold)) {
        START_COLOR("color_bad");
        colorful_output = true;
    }

    for (walk = format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "free")) {
            outwalk += print_bytes_human(outwalk, (uint64_t)buf.f_bsize * (uint64_t)buf.f_bfree, prefix_type);
            walk += strlen("free");
        }

        if (BEGINS_WITH(walk + 1, "used")) {
            outwalk += print_bytes_human(outwalk, (uint64_t)buf.f_bsize * ((uint64_t)buf.f_blocks - (uint64_t)buf.f_bfree), prefix_type);
            walk += strlen("used");
        }

        if (BEGINS_WITH(walk + 1, "total")) {
            outwalk += print_bytes_human(outwalk, (uint64_t)buf.f_bsize * (uint64_t)buf.f_blocks, prefix_type);
            walk += strlen("total");
        }

        if (BEGINS_WITH(walk + 1, "avail")) {
            outwalk += print_bytes_human(outwalk, (uint64_t)buf.f_bsize * (uint64_t)buf.f_bavail, prefix_type);
            walk += strlen("avail");
        }

        if (BEGINS_WITH(walk + 1, "percentage_free")) {
            outwalk += sprintf(outwalk, "%.01f%s", 100.0 * (double)buf.f_bfree / (double)buf.f_blocks, pct_mark);
            walk += strlen("percentage_free");
        }

        if (BEGINS_WITH(walk + 1, "percentage_used_of_avail")) {
            outwalk += sprintf(outwalk, "%.01f%s", 100.0 * (double)(buf.f_blocks - buf.f_bavail) / (double)buf.f_blocks, pct_mark);
            walk += strlen("percentage_used_of_avail");
        }

        if (BEGINS_WITH(walk + 1, "percentage_used")) {
            outwalk += sprintf(outwalk, "%.01f%s", 100.0 * (double)(buf.f_blocks - buf.f_bfree) / (double)buf.f_blocks, pct_mark);
            walk += strlen("percentage_used");
        }

        if (BEGINS_WITH(walk + 1, "percentage_avail")) {
            outwalk += sprintf(outwalk, "%.01f%s", 100.0 * (double)buf.f_bavail / (double)buf.f_blocks, pct_mark);
            walk += strlen("percentage_avail");
        }
    }

    if (colorful_output)
        END_COLOR;

    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);
}
