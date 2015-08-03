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

/*
 * Check whether path references regular file, and if it does, check access
 * and modification times
 *
 */
void print_mbox_info(yajl_gen json_gen, char *buffer, const char *path, const char *format, const char *format_no_mail) {
    char *outwalk = buffer, *p;
    struct stat sb;
    struct timeval times[2];
    bool exists = !stat(path, &sb);
    FILE *f;
    char s[LINELEN];
    size_t messages = 0;

    INSTANCE(path);

    if (exists && S_ISREG(sb.st_mode) && sb.st_size != 0 && sb.st_atime <= sb.st_mtime) {
        /* Read the file and count the "From" header instances */
        if ((f = fopen(path, "rw")) == NULL) {
            START_COLOR("color_bad");
            outwalk += sprintf(outwalk, "mbox: ");
            (void)strerror_r(errno, outwalk, sizeof(buffer) - (outwalk - buffer));
            outwalk = buffer + strnlen(buffer, sizeof(buffer));
            goto out;
        }
        while (fgets(s, LINELEN, f) != NULL) {
            if (strncmp(s, "From:", 5) == 0)
                messages++;
            if (strncmp(s, "Status: RO", 10) == 0)
                messages--;
        }

        /* If fgets(3) returned NULL due to some error, message count
         * is likely wrong.  Print error instead of message count */
        if (!feof(f)) {
            (void)fclose(f);
            START_COLOR("color_bad");
            outwalk += sprintf(outwalk, "mbox: ");
            (void)strerror_r(errno, outwalk, sizeof(buffer) - (outwalk - buffer));
            outwalk = buffer + strnlen(buffer, sizeof(buffer));
            goto out;
        }
        (void)fclose(f);

        /* Restore access and modification times from stat(2) call
         * and apply back to the file */
        TIMESPEC_TO_TIMEVAL(&times[0], &(sb.st_atim))
        TIMESPEC_TO_TIMEVAL(&times[1], &sb.st_mtim)
        (void)utimes(path, times);

        START_COLOR("color_degraded");
        for (*outwalk = '\0'; (p = strstr(format, "%mails")) != NULL; format = p + 6) {
            (void)strncat(buffer, format, p - format);
            outwalk += p - format;
            outwalk += sprintf(outwalk, "%zu", messages);
        }
        outwalk += sprintf(outwalk, "%s", format);
    } else if (exists && !S_ISREG(sb.st_mode)) {
        START_COLOR("color_bad");
        outwalk += sprintf(outwalk, "%s is not an mbox", path);
    } else if (format_no_mail) {
        START_COLOR("color_good");
        outwalk += sprintf(outwalk, "%s", format_no_mail);
    }

out:
    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
