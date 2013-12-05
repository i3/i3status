// vim:ts=8:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || (__OpenBSD__) || defined(__DragonFly__)
#include <sys/param.h>
#include <sys/mount.h>
#endif
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#define BINARY_BASE UINT64_C(1024)
#define DECIMAL_BASE UINT64_C(1000)

#define MAX_EXPONENT 4

static const char * const iec_symbols[MAX_EXPONENT+1] = {"", "Ki", "Mi", "Gi", "Ti"};
static const char * const si_symbols[MAX_EXPONENT+1] = {"", "k", "M", "G", "T"};
static const char * const custom_symbols[MAX_EXPONENT+1] = {"", "K", "M", "G", "T"};

/*
 * Formats bytes according to the given base and set of symbols.
 *
 */
static int format_bytes(char *outwalk, uint64_t bytes, uint64_t base, const char * const symbols[]) {
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
        if (strncmp(prefix_type, "decimal", strlen(prefix_type)) == 0) {
                return format_bytes(outwalk, bytes, DECIMAL_BASE, si_symbols);
        } else if (strncmp(prefix_type, "custom", strlen(prefix_type)) == 0) {
                return format_bytes(outwalk, bytes, BINARY_BASE, custom_symbols);
        } else {
                return format_bytes(outwalk, bytes, BINARY_BASE, iec_symbols);
        }
}

/*
 * Does a statvfs and prints either free, used or total amounts of bytes in a
 * human readable manner.
 *
 */
void print_disk_info(yajl_gen json_gen, char *buffer, const char *path, const char *format, const char *prefix_type) {
        const char *walk;
        char *outwalk = buffer;

        INSTANCE(path);

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__) || defined(__DragonFly__)
        struct statfs buf;

        if (statfs(path, &buf) == -1)
                return;
#else
        struct statvfs buf;

        if (statvfs(path, &buf) == -1)
                return;
#endif

        for (walk = format; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        *(outwalk++) = *walk;
                        continue;
                }

                if (BEGINS_WITH(walk+1, "free")) {
                        outwalk += print_bytes_human(outwalk, (uint64_t)buf.f_bsize * (uint64_t)buf.f_bfree, prefix_type);
                        walk += strlen("free");
                }

                if (BEGINS_WITH(walk+1, "used")) {
                        outwalk += print_bytes_human(outwalk, (uint64_t)buf.f_bsize * ((uint64_t)buf.f_blocks - (uint64_t)buf.f_bfree), prefix_type);
                        walk += strlen("used");
                }

                if (BEGINS_WITH(walk+1, "total")) {
                        outwalk += print_bytes_human(outwalk, (uint64_t)buf.f_bsize * (uint64_t)buf.f_blocks, prefix_type);
                        walk += strlen("total");
                }

                if (BEGINS_WITH(walk+1, "avail")) {
                        outwalk += print_bytes_human(outwalk, (uint64_t)buf.f_bsize * (uint64_t)buf.f_bavail, prefix_type);
                        walk += strlen("avail");
                }

                if (BEGINS_WITH(walk+1, "percentage_free")) {
                        outwalk += sprintf(outwalk, "%.01f%%", 100.0 * (double)buf.f_bfree / (double)buf.f_blocks);
                        walk += strlen("percentage_free");
                }

                if (BEGINS_WITH(walk+1, "percentage_used_of_avail")) {
                        outwalk += sprintf(outwalk, "%.01f%%", 100.0 * (double)(buf.f_blocks - buf.f_bavail) / (double)buf.f_blocks);
                        walk += strlen("percentage_used_of_avail");
                }

                if (BEGINS_WITH(walk+1, "percentage_used")) {
                        outwalk += sprintf(outwalk, "%.01f%%", 100.0 * (double)(buf.f_blocks - buf.f_bfree) / (double)buf.f_blocks);
                        walk += strlen("percentage_used");
                }

                if (BEGINS_WITH(walk+1, "percentage_avail")) {
                        outwalk += sprintf(outwalk, "%.01f%%", 100.0 * (double)buf.f_bavail / (double)buf.f_blocks);
                        walk += strlen("percentage_avail");
                }
        }

        *outwalk = '\0';
        OUTPUT_FULL_TEXT(buffer);
}
