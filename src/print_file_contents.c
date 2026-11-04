// vim:ts=4:sw=4:expandtab

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "i3status.h"

#define STRING_SIZE 10

void print_file_contents(file_contents_ctx_t *ctx) {
    const char *walk = ctx->format;
    char *outwalk = ctx->buf;

    if (ctx->path == NULL) {
        OUTPUT_FULL_TEXT("error: path not configured");
        return;
    }

    /* A negative max_characters would otherwise turn into a gigantic
     * allocation. */
    const size_t max_chars = ctx->max_chars > 0 ? (size_t)ctx->max_chars : 0;
    char *buf = scalloc(max_chars + 1);

    char *abs_path = resolve_tilde(ctx->path);
    int fd = open(abs_path, O_RDONLY);
    /* Remember the error right away: it is what %errno and %error show, and
     * nothing that runs later may be allowed to overwrite it. */
    int saved_errno = (fd == -1) ? errno : 0;
    free(abs_path);

    INSTANCE(ctx->path);

    if (fd != -1) {
        ssize_t n = read(fd, buf, max_chars);
        if (n == -1) {
            saved_errno = errno;
        } else {
            buf[n] = '\0';
        }
        (void)close(fd);
    }

    if (saved_errno == 0) {
        START_COLOR("color_good");
    } else {
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

    snprintf(string_errno, sizeof(string_errno), "%d", saved_errno);

    placeholder_t placeholders[] = {
        {.name = "%title", .value = ctx->title},
        {.name = "%content", .value = buf},
        {.name = "%errno", .value = string_errno},
        {.name = "%error", .value = strerror(saved_errno)}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    char *formatted = format_placeholders(walk, &placeholders[0], num);
    OUTPUT_FORMATTED;
    free(formatted);
    free(buf);

    END_COLOR;
    OUTPUT_FULL_TEXT(ctx->buf);
}
