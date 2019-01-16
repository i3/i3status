// vim:ts=4:sw=4:expandtab

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <sys/types.h>

#include <sys/fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "i3status.h"

static void *scalloc(size_t size) {
    void *result = calloc(size, 1);
    if (result == NULL) {
        die("Error: out of memory (calloc(%zu))\n", size);
    }
    return result;
}

void print_file_contents(yajl_gen json_gen, char *buffer, const char *title, const char *path, const char *format, const char *format_bad, const int max_chars) {
    const char *walk = format;
    char *outwalk = buffer;
    char *buf = scalloc(max_chars * sizeof(char));

    int n = -1;
    int fd = open(path, O_RDONLY);

    INSTANCE(path);

    if (fd > -1) {
        n = read(fd, buf, max_chars);
        if (n != -1) {
            buf[n] = '\0';
        }
        (void)close(fd);
        START_COLOR("color_good");
    } else if (errno != 0) {
        walk = format_bad;
        START_COLOR("color_bad");
    }

    for (; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
        } else if (BEGINS_WITH(walk + 1, "title")) {
            outwalk += sprintf(outwalk, "%s", title);
            walk += strlen("title");
        } else if (BEGINS_WITH(walk + 1, "content")) {
            for (char *s = buf; *s != '\0' && n > 0; s++, n--) {
                if (*s != '\n') {
                    *(outwalk++) = *s;
                }
            }
            walk += strlen("content");
        } else if (BEGINS_WITH(walk + 1, "errno")) {
            outwalk += sprintf(outwalk, "%d", errno);
            walk += strlen("errno");
        } else if (BEGINS_WITH(walk + 1, "error")) {
            outwalk += sprintf(outwalk, "%s", strerror(errno));
            walk += strlen("error");
        } else {
            *(outwalk++) = '%';
        }
    }

    free(buf);

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
