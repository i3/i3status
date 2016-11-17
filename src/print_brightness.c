#include <stdlib.h>
#include <glob.h>

#include "i3status.h"

static void expand_glob(const char *path, char **expanded_path) {
    static glob_t glob_path;
    if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &glob_path) < 0)
        die("glob() failed on %s\n", path);
    if (glob_path.gl_pathc == 0)
        asprintf(expanded_path, "%s", path);
    else
        asprintf(expanded_path, "%s", glob_path.gl_pathv[0]);
    globfree(&glob_path);
}

static long read_value(const char *path) {
    char buf[16];
    if (!slurp(path, buf, sizeof(buf)))
        return false;
    return strtol(buf, NULL, 10);
}

/**
 * Reads the actual and max brightness values and returns the level as percentage.
 *
 */
void print_brightness(yajl_gen json_gen, char *buffer, const char *format, const char *actual_brightness_path, const char *max_brightness_path) {
    char *outwalk = buffer;
    const char *walk;
    char *actual_path_expanded, *max_path_expanded;
    long actual_value, max_value;

    expand_glob(actual_brightness_path, &actual_path_expanded);
    expand_glob(max_brightness_path, &max_path_expanded);

    actual_value = read_value(actual_path_expanded);
    max_value = read_value(max_path_expanded);

    if (actual_value > 0 && max_value > 0 && actual_value <= max_value) {
        for (walk = format; *walk != '\0'; walk++) {
            if (*walk != '%') {
                *(outwalk++) = *walk;
                continue;
            }
            if (BEGINS_WITH(walk + 1, "brightness")) {
                int brightness = ((float)actual_value / max_value) * 100;
                outwalk += sprintf(outwalk, "%02d%s", brightness, pct_mark);
                walk += strlen("brightness");
            }
        }

        OUTPUT_FULL_TEXT(buffer);
    } else {
        OUTPUT_FULL_TEXT("invalid brightness values");
        (void)fputs("i3status: Invalid brightness values\n", stderr);
    }
}
