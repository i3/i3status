// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include "i3status.h"

void print_hostname(yajl_gen json_gen, char *buffer, const char *format) {
    char *outwalk = buffer;

    const char *selected_format = format;
    const char *walk;
    const char hostname[128];
    (void)memset((void *)&hostname, 0, (128 * sizeof(char)));

    int err = gethostname((char *)&hostname, 128);

    if (err != 0) {
        strcpy((char *)&hostname, "No Hostname");
        (void)fputs("i3status: Cannot read hostname.\n", stderr);
    }

    for (walk = selected_format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }
        if (BEGINS_WITH(walk + 1, "hostname")) {
            outwalk += sprintf(outwalk, "%s", hostname);
            walk += strlen("hostname");
        }
    }

    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);

    return;
}
