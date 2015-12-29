// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <sys/stat.h>
#include <stdlib.h>
#include "i3status.h"

void print_file_contents(yajl_gen json_gen, char *buffer, const char *title, const char *path, const char *format) {
    struct stat st;
    const char *walk;
    char *outwalk = buffer;
    char *fcontents = NULL;
    const bool exists = (stat(path, &st) == 0);

    if(exists) {
      FILE *fd = fopen(path, "r");
      fcontents = malloc(st.st_size * sizeof(char));
      int res = fread(fcontents, sizeof(char), st.st_size, fd);
      if(res != -1)
        fcontents[res] = '\0';
      (void)fclose(fd);
    }

    INSTANCE(path);

    START_COLOR((exists ? "color_good" : "color_bad"));

    for (walk = format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }
        if (BEGINS_WITH(walk + 1, "title")) {
            outwalk += sprintf(outwalk, "%s", title);
            walk += strlen("title");
        } else if (BEGINS_WITH(walk + 1, "contents")) {
            outwalk += sprintf(outwalk, "%s", (exists ? fcontents : "?"));
            walk += strlen("contents");
        }
    }

    if(exists) free(fcontents);

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
