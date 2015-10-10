// vim:ts=4:sw=4:expandtab
#include <glob.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

static bool file_read_number(const char *path, float *out) {
    static char contentbuf[64];
    if (!out)
        die("out == NULL\n");
    if (!slurp(path, contentbuf, sizeof(contentbuf)))
        return false;
    *out = strtof(contentbuf, NULL);
    return true;
}

static bool glob_read_number(const char *path, float *out) {
    static glob_t globbuf;
    if (!out)
        die("out == NULL\n");
    if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
        die("glob() failed\n");
    if (globbuf.gl_pathc == 0) {
        globfree(&globbuf);
        // No glob matches, the specified path does not contain a wildcard.
        return file_read_number(path, out);
    }
    for (size_t i = 0; i < globbuf.gl_pathc; i++) {
        if (file_read_number(globbuf.gl_pathv[i], out)) {
            globfree(&globbuf);
            return true;
        }
    }
    globfree(&globbuf);
    return false;
}

/**
 * Computes the ratio of two numbers in files and displays them.
 *
 */
void print_ratio_watch(yajl_gen json_gen, char *buffer, const char *title, const char *format_down, const char *format_high, const char *format_low, const char *path_numerator, const char *path_denominator, const float low_threshold) {
    char *outwalk = buffer;
    float numerator = 0.0;
    float denominator = 0.0;

    if (!glob_read_number(path_numerator, &numerator) ||
        !glob_read_number(path_denominator, &denominator)) {
        START_COLOR("color_bad");
        outwalk += sprintf(outwalk, "%s", format_down);
        END_COLOR;
        OUTPUT_FULL_TEXT(buffer);
        return;
    }
    float percentage = (numerator / denominator) * 100.0;
    const char *walk = percentage >= low_threshold ? format_high : format_low;

    for (; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "percentage")) {
            outwalk += sprintf(outwalk, "%.00f%s", percentage, pct_mark);
            walk += strlen("percentage");
        }
    }

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
