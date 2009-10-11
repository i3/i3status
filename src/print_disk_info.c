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

void print_bytes_human(uint64_t bytes) {
        /* 1 TB */
        if (bytes > TERABYTE)
                printf("%f TB", (double)bytes / TERABYTE);
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
 * Just parses /proc/net/wireless looking for lines beginning with
 * wlan_interface, extracting the quality of the link and adding the
 * current IP address of wlan_interface.
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
                        print_bytes_human(buf.f_bsize * buf.f_bfree);
                        walk += strlen("free");
                }

                if (BEGINS_WITH(walk+1, "used")) {
                        print_bytes_human(buf.f_bsize * (buf.f_blocks - buf.f_bfree));
                        walk += strlen("used");
                }

                if (BEGINS_WITH(walk+1, "total")) {
                        print_bytes_human(buf.f_bsize * buf.f_blocks);
                        walk += strlen("total");
                }
        }
}
