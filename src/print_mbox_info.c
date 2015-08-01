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
void print_mbox_info(yajl_gen json_gen, char *buffer, const char *path, const char *prefix, const char *postfix, const char *label, const char *label_no_mail) {
    char *outwalk = buffer;
    struct stat sb;
    int exists = !stat(path, &sb);

    INSTANCE(path);

    if (!prefix)
        prefix = "";

    if (!postfix)
        postfix = "";

    if (exists && S_ISREG(sb.st_mode) && sb.st_size != 0 && sb.st_atime <= sb.st_mtime) {
        START_COLOR("color_degraded");
        outwalk += sprintf(outwalk, "%s%s%s", prefix, label ? label : basename(path), postfix);
    } else if (exists && !S_ISREG(sb.st_mode)) {
        START_COLOR("color_bad");
        outwalk += sprintf(outwalk, "%s is not an mbox", path);
    } else if (label_no_mail) {
        START_COLOR("color_good");
        outwalk += sprintf(outwalk, "%s%s%s", prefix, label_no_mail, postfix);
    }

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
