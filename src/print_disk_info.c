// vim:ts=8:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/statvfs.h>
#include <sys/types.h>

#include "i3status.h"

#define TERABYTE (1024ULL * 1024 * 1024 * 1024)
#define GIGABYTE (1024ULL * 1024 * 1024)
#define MEGABYTE (1024ULL * 1024)
#define KILOBYTE (1024ULL)

/*
 * Prints the given amount of bytes in a human readable manner.
 *
 */
static void print_bytes_human(uint64_t bytes) {
        if (bytes > TERABYTE)
                printf("%.02f TB", (double)bytes / TERABYTE);
        else if (bytes > GIGABYTE)
                printf("%.01f GB", (double)bytes / GIGABYTE);
        else if (bytes > MEGABYTE)
                printf("%.01f MB", (double)bytes / MEGABYTE);
        else if (bytes > KILOBYTE)
                printf("%.01f KB", (double)bytes / KILOBYTE);
        else {
                printf("%.01f B", (double)bytes);
        }
}

/*
 * Does a statvfs and prints either free, used or total amounts of bytes in a
 * human readable manner.
 *
 */
void print_disk_info(const char *path, const char *format) {
        const char *walk;
        struct statvfs buf;

        if (statvfs(path, &buf) == -1)
                return;

        for (walk = format; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        putchar(*walk);
                        continue;
                }

                if (BEGINS_WITH(walk+1, "free")) {
                        print_bytes_human((uint64_t)buf.f_bsize * (uint64_t)buf.f_bfree);
                        walk += strlen("free");
                }

                if (BEGINS_WITH(walk+1, "used")) {
                        print_bytes_human((uint64_t)buf.f_bsize * ((uint64_t)buf.f_blocks - (uint64_t)buf.f_bfree));
                        walk += strlen("used");
                }

                if (BEGINS_WITH(walk+1, "total")) {
                        print_bytes_human((uint64_t)buf.f_bsize * (uint64_t)buf.f_blocks);
                        walk += strlen("total");
                }

                if (BEGINS_WITH(walk+1, "avail")) {
                        print_bytes_human((uint64_t)buf.f_bsize * (uint64_t)buf.f_bavail);
                        walk += strlen("avail");
                }
        }
}
