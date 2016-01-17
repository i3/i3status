// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#define BUFSIZE 1024

void print_cmd(yajl_gen json_gen, char *buffer, const char *cmd) {
    char *outwalk = buffer;
    char path[BUFSIZE];
    FILE *fp;
    fp = popen(cmd, "r");
    fgets(path, sizeof(path) - 1, fp);
    pclose(fp);

    char *nl = index(path, '\n');
    if (nl != NULL) {
        *nl = '\0';
    }
    maybe_escape_markup(path, &outwalk);
    OUTPUT_FULL_TEXT(buffer);
}
