// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <libgen.h>
#include <sys/stat.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

/*
 * Check whether path references regular file, and if it does, check access
 * and modification times
 *
 */
void print_mbox_info(yajl_gen json_gen, char *buffer, const char *path, const char *prefix, const char *label) {
    char *outwalk = buffer;
    struct stat sb;

    INSTANCE(path);

    if (stat(path, &sb) == 0 && S_ISREG(sb.st_mode) && sb.st_size != 0) {
        if (sb.st_atime <= sb.st_mtime) {
            START_COLOR("color_bad");
            outwalk += sprintf(outwalk, "%s", prefix);
            if (strcmp(label, "default") == 0) {
                outwalk += sprintf(outwalk, "%s", basename(path));
            } else {
                outwalk += sprintf(outwalk, "%s", label);
            }
            END_COLOR;
        }
    }

    OUTPUT_FULL_TEXT(buffer);
}
