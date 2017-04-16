// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <sys/stat.h>
#include "i3status.h"

void print_file(yajl_gen json_gen, char *buffer, const char *title, const char *path) {
    char *outwalk = buffer;
    outwalk[0] = '\0';

    if(title != NULL)
        INSTANCE(title);

    slurp(path, outwalk, 4096);
    outwalk += (strnlen(outwalk, 4096-1) + 1);
    OUTPUT_FULL_TEXT(buffer);
}
