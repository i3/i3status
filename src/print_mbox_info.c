// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#define LINELEN 1000

/* These variables are used to store data between calls to print_mbox_info(). */
static struct timeval times[2] = { 0 };
static size_t messages = 0;

/*
 * Check whether path references regular file, and if it does, parse the file,
 * search for continuous blocks of text starting with unquoted "From " line.
 * Assume that such blocks are mail headers, and search "Status" header with
 * "RO" body.  Assume that messages with such header are read, and substract
 * them from total number of messages.
 * When done, restore mtime and atime attributes of mbox, so that it remains
 * visible on next check.
 *
 */
void print_mbox_info(yajl_gen json_gen, char *buffer, const char *path, const char *format, const char *format_no_mail) {
    char *outwalk = buffer;
    const char *walk = format;
    struct stat sb;
    bool exists = !stat(path, &sb), mtime_changed = (sb.st_mtim.tv_sec != times[1].tv_sec);
    bool after_blank_line = true, in_header = false;
    FILE *f;
    char s[LINELEN];

    INSTANCE(path);

    if (exists && !S_ISREG(sb.st_mode)) {
        START_COLOR("color_bad");
        outwalk += sprintf(outwalk, "%s is not an mbox", path);
        END_COLOR;
        goto out;
    }

    if (!exists || sb.st_size == 0 || (!mtime_changed && messages == 0)) {
        if (format_no_mail) {
            START_COLOR("color_good");
            outwalk += sprintf(outwalk, "%s", format_no_mail);
            END_COLOR;
        }
        goto out;
    }

    /* If modification time didn't change, apparently the contents is
     * the same; avoid disk operations then */
    if (mtime_changed) {
        messages = 0;
        /* Read the file and count the "From" header instances */
        if ((f = fopen(path, "rw")) == NULL) {
            START_COLOR("color_bad");
            outwalk += sprintf(outwalk, "mbox: ");
            (void)strerror_r(errno, outwalk, sizeof(buffer) - (outwalk - buffer));
            outwalk = buffer + strnlen(buffer, sizeof(buffer));
            END_COLOR;
            goto out;
        }
        while (fgets(s, LINELEN, f) != NULL) {
            if (after_blank_line && BEGINS_WITH(s, "From ")) {
                messages++;
                in_header = true;
            } else if (in_header && BEGINS_WITH(s, "Status:") && strchr(s, 'O') != NULL) {
                messages--;
            }

            if (*s == '\n') {
                after_blank_line = true;
                in_header = false;
            } else {
                after_blank_line = false;
            }
        }

        /* If fgets(3) returned NULL due to some error, message count
         * is likely wrong.  Print error instead of message count */
        if (!feof(f)) {
            (void)fclose(f);
            START_COLOR("color_bad");
            outwalk += sprintf(outwalk, "mbox: ");
            (void)strerror_r(errno, outwalk, sizeof(buffer) - (outwalk - buffer));
            outwalk = buffer + strnlen(buffer, sizeof(buffer));
            END_COLOR;
            goto out;
        }
        (void)fclose(f);

        /* Restore access and modification times from stat(2) call
         * and apply back to the file */
        TIMESPEC_TO_TIMEVAL(&times[0], &(sb.st_atim))
        TIMESPEC_TO_TIMEVAL(&times[1], &sb.st_mtim)
        (void)utimes(path, times);
    }
    if (messages == 0) {
        if (format_no_mail) {
            START_COLOR("color_good");
            outwalk += sprintf(outwalk, "%s", format_no_mail);
            END_COLOR;
        }
    } else {
        START_COLOR("color_degraded");
        for (; *walk != '\0'; walk++) {
            if (*walk != '%') {
                *(outwalk++) = *walk;
                continue;
            }

            if (BEGINS_WITH(walk + 1, "mails")) {
                outwalk += sprintf(outwalk, "%zu", messages);
                walk += strlen("mails");
            }

            if (BEGINS_WITH(walk + 1, "%")) {
                outwalk += sprintf(outwalk, "%%");
                walk += strlen("%");
            }
        }
        END_COLOR;
    }

out:
    OUTPUT_FULL_TEXT(buffer);
}
