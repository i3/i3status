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

#define TERABYTE (1024ULL * 1024 * 1024 * 1024)
#define GIGABYTE (1024ULL * 1024 * 1024)
#define MEGABYTE (1024ULL * 1024)
#define KILOBYTE (1024ULL)

/*
 * Prints the given amount of bytes in a human readable manner.
 *
 */
static int print_bytes_human(char *outwalk, uint64_t bytes) {
        if (bytes > TERABYTE)
                return sprintf(outwalk, "%.02f TB", (double)bytes / TERABYTE);
        else if (bytes > GIGABYTE)
                return sprintf(outwalk, "%.01f GB", (double)bytes / GIGABYTE);
        else if (bytes > MEGABYTE)
                return sprintf(outwalk, "%.01f MB", (double)bytes / MEGABYTE);
        else if (bytes > KILOBYTE)
                return sprintf(outwalk, "%.01f KB", (double)bytes / KILOBYTE);
        else {
                return sprintf(outwalk, "%.01f B", (double)bytes);
        }
}

/*
 * Does a statvfs and prints either free, used or total amounts of bytes in a
 * human readable manner.
 *
 */
void print_disk_info(yajl_gen json_gen, char *buffer, const char *path, const char *format) {
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
                        outwalk += print_bytes_human(outwalk, (uint64_t)buf.f_bsize * (uint64_t)buf.f_bfree);
                        walk += strlen("free");
                }

                if (BEGINS_WITH(walk+1, "used")) {
                        outwalk += print_bytes_human(outwalk, (uint64_t)buf.f_bsize * ((uint64_t)buf.f_blocks - (uint64_t)buf.f_bfree));
                        walk += strlen("used");
                }

                if (BEGINS_WITH(walk+1, "total")) {
                        outwalk += print_bytes_human(outwalk, (uint64_t)buf.f_bsize * (uint64_t)buf.f_blocks);
                        walk += strlen("total");
                }

                if (BEGINS_WITH(walk+1, "avail")) {
                        outwalk += print_bytes_human(outwalk, (uint64_t)buf.f_bsize * (uint64_t)buf.f_bavail);
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
