// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

void print_exec(yajl_gen json_gen, char *buffer, const char *command) {
    char *outwalk = buffer;

    FILE *fproc = popen(command, "r");
    char buf[256];
    while (!feof(fproc)) {
        size_t rd = fread(buf, 1, 255, fproc);
        snprintf(outwalk, rd + 1, "%s", buf);
        outwalk += rd;
    }
    pclose(fproc);

    while (*(outwalk - 1) == '\n')
        outwalk--;

    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);
}
