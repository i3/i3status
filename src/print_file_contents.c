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

void print_file_contents(file_contents_ctx_t *ctx) {
    const char *walk = ctx->format;
    char *outwalk = ctx->buf;
    char *buf = scalloc(ctx->max_chars * sizeof(char) + 1);

    if (ctx->path == NULL) {
        OUTPUT_FULL_TEXT("error: path not configured");
        free(buf);
        return;
    }

    char *abs_path = resolve_tilde(ctx->path);
    int fd = open(abs_path, O_RDONLY);
    free(abs_path);

    INSTANCE(ctx->path);

    int n = -1;
    if (fd > -1) {
        n = read(fd, buf, ctx->max_chars);
        if (n != -1) {
            buf[n] = '\0';
        }
        (void)close(fd);
        START_COLOR("color_good");
    } else if (errno != 0) {
        walk = ctx->format_bad;
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
        {.name = "%title", .value = ctx->title},
        {.name = "%content", .value = buf},
        {.name = "%errno", .value = string_errno},
        {.name = "%error", .value = strerror(errno)}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    char *formatted = format_placeholders(walk, &placeholders[0], num);
    OUTPUT_FORMATTED;
    free(formatted);
    free(buf);

    END_COLOR;
    OUTPUT_FULL_TEXT(ctx->buf);
}
