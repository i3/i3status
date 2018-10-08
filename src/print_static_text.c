// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include "i3status.h"

void print_static_text(yajl_gen json_gen, char *buffer, const char *title, const char *pidfile, const char *format, const char *format_down)
{
    static long long last_mtime;
    static char *contents;
    const char *walk;
    char *outwalk = buffer;
    struct stat sb;
    bool found;

    found = stat(pidfile, &sb) == 0;
    if (found && sb.st_mtime < last_mtime)
	return;

    if (found) {
	free(contents);
	contents = malloc(sb.st_size + 1);
	if (!slurp(pidfile, contents, sb.st_size + 1))
	    return;
	last_mtime = sb.st_mtime;
    }

    if (found || format_down == NULL) {
        walk = format;
    } else {
        walk = format_down;
    }

    INSTANCE(pidfile);

    START_COLOR((*contents ? "color_good" : "color_bad"));

    for (; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;

        } else if (BEGINS_WITH(walk + 1, "title")) {
            outwalk += sprintf(outwalk, "%s", title);
            walk += strlen("title");

        } else if (BEGINS_WITH(walk + 1, "status")) {
            outwalk += sprintf(outwalk, "%s", (found
					       ? (*contents ? "exists" : "empty")
					       : "missing"));
            walk += strlen("status");

	} else if (BEGINS_WITH(walk + 1, "contents")) {
            outwalk += sprintf(outwalk, "%s", *contents ? contents : "*");
            walk += strlen("contents");

        } else {
            *(outwalk++) = '%';
        }
    }

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
