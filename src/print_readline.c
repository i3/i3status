// vim:ts=4:sw=4:expandtab
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <sys/stat.h>
#include "i3status.h"

void print_readline(yajl_gen json_gen, char *buffer, const char *title, const char *path, const char *format, const char *format_down) {
    const char *walk;
    char *outwalk = buffer, line[256], *ptr;

    memset(line, 0, sizeof(line));

    int fd = open(path, O_RDONLY);
    if (fd >= 0 || format_down == NULL) {
        walk = format;
        read(fd, line, sizeof(line) - 1);
        close(fd);
    } else {
        walk = format_down;
    }

    // Sanitize the line.
    for (const char *in = ptr = line; *in != 0; in++) {
        // End at newlines.
        if (*in == '\r' || *in == '\n') {
            *ptr = 0;
            break;
        }

        // Replace non-printable characters by whitespaces, but do not
        // emit multiple consecutive whitespaces.
        if (isprint(*in) == 0) {
            if (ptr != line && ptr[-1] != ' ') {
                *ptr++ = ' ';
            }
        } else {
            *ptr++ = *in;
        }
    }

    // Strip a trailing whitespace.
    if (*ptr == ' ') {
        ptr--;
    }

    *ptr = 0;

    INSTANCE(path);

    START_COLOR((ptr != line ? "color_good" : "color_bad"));

    for (; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "title")) {
            outwalk += sprintf(outwalk, "%s", title);
            walk += strlen("title");
        } else if (BEGINS_WITH(walk + 1, "status")) {
            outwalk += sprintf(outwalk, "%s", ptr != line ? line : "no");
            walk += strlen("status");
        }
    }

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
