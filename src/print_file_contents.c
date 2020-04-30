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

#define STRING_SIZE 10

static void *scalloc(size_t size) {
    void *result = calloc(size, 1);
    if (result == NULL) {
        die("Error: out of memory (calloc(%zu))\n", size);
    }
    return result;
}

void print_file_contents(yajl_gen json_gen, char *buffer, const char *title, const char *path, const char *format, const char *format_bad, const int max_chars) {
    const char *walk = format;
    /* Each utf-8 character is a maximum of 4 bytes */
    const int max_bytes = 4 * max_chars;
    char *outwalk = buffer;
    char *buf = scalloc(max_bytes * sizeof(char) + 1);

    int fd = open(path, O_RDONLY);

    INSTANCE(path);

    if (fd > -1) {
        read(fd, buf, max_bytes);

        int i = 0;
        int actual_chars = 0;
        while (i < max_bytes) {
            const unsigned char c = buf[i];

            if (c == 0) {
                break;
            }
            /* Count utf-8 characters, by only inspecting the first byte,
             * skipping the rest. For reference see:
             * https://tools.ietf.org/html/rfc3629#page-4 */
            if (c < 128 /* 0xxxxxxx */) {
                i++;
            } else if (c < 0xe0 /* 110xxxxx */) {
                i += 2;
            } else if (c < 0xf0 /* 1110xxxx */) {
                i += 3;
            } else if (c < 0xf5 /* 11110xxx */) {
                i += 4;
            }
            if (++actual_chars >= max_chars) {
                buf[min(i, max_bytes)] = '\0';
                break;
            }
        }

        (void)close(fd);
        START_COLOR("color_good");
    } else if (errno != 0) {
        walk = format_bad;
        START_COLOR("color_bad");
    }

    // remove newline chars
    char *src, *dst;
    for (src = dst = buf; *src != '\0'; src++) {
        *dst = *src;
        if (*dst != '\n') {
            dst++;
        }
    }
    *dst = '\0';

    char string_errno[STRING_SIZE];

    sprintf(string_errno, "%d", errno);

    placeholder_t placeholders[] = {
        {.name = "%title", .value = title},
        {.name = "%content", .value = buf},
        {.name = "%errno", .value = string_errno},
        {.name = "%error", .value = strerror(errno)}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    buffer = format_placeholders(walk, &placeholders[0], num);

    free(buf);

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
    free(buffer);
}
