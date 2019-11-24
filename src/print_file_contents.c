// vim:ts=4:sw=4:expandtab

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <sys/types.h>

#include <wordexp.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "i3status.h"

void print_file_contents(yajl_gen json_gen, char *buffer, const char *title, const char *path, const char *format, const char *format_bad, const int max_chars) {
    errno = 0;

    wordexp_t we;
    if (wordexp(path, &we, 0) != 0)
        die("Failed to expand path: %s.\n");
    if (we.we_wordc != 1)
        die("Path expanded into %d words: %s.\n"
            "Expected one word. Please make sure all spaces are escaped.\n",
            we.we_wordc, path);
    FILE *f = fopen(we.we_wordv[0], "r");
    wordfree(&we);

    char *linebuf = NULL;
    if (f != NULL) {
        linebuf = malloc(max_chars * sizeof(char) + 1);
        if (linebuf != NULL) {
            const size_t nread = fread(linebuf, sizeof(char), max_chars, f);
            linebuf[nread] = '\0';
        }
        fclose(f);
    }

    INSTANCE(path);

    const char *walk;
    char *outwalk = buffer;
    if (linebuf != NULL) {
        walk = format;
        START_COLOR("color_good");
    } else {
        walk = format_bad;
        START_COLOR("color_bad");
    }

    while (*walk != '\0') {
        if (BEGINS_WITH(walk, "%title")) {
            walk += strlen("%title");
            outwalk += sprintf(outwalk, "%s", title);
        } else if (BEGINS_WITH(walk, "%content")) {
            walk += strlen("%content");
            if (linebuf != NULL) {
                const size_t linelen = strcspn(linebuf, "\r\n");
                memcpy(outwalk, linebuf, linelen);
                outwalk += linelen;
            }
        } else if (BEGINS_WITH(walk, "%errno")) {
            walk += strlen("%errno");
            outwalk += sprintf(outwalk, "%d", errno);
        } else if (BEGINS_WITH(walk, "%error")) {
            walk += strlen("%error");
            outwalk += sprintf(outwalk, "%s", strerror(errno));
        } else {
            *outwalk++ = *walk++;
        }
    }

    free(linebuf);

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
