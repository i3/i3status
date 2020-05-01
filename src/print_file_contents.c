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

void print_file_contents(yajl_gen json_gen, char *buffer, const char *title, const char *path, const char *format, const char *format_bad, const int max_chars) {
    const char *walk = format;
    char *outwalk = buffer;
    char *buf = scalloc(max_chars * sizeof(char) + 1);

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
