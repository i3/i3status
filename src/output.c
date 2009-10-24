// vim:ts=8:expandtab
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>

#include "i3status.h"

/*
 * Returns the correct color format for dzen (^fg(color)) or xmobar (<fc=color>)
 *
 */
char *color(const char *colorstr) {
        static char colorbuf[32];
        if (!cfg_getbool(cfg_general, "colors")) {
                colorbuf[0] = '\0';
                return colorbuf;
        }
        if (output_format == O_DZEN2)
                (void)snprintf(colorbuf, sizeof(colorbuf), "^fg(%s)", colorstr);
        else if (output_format == O_XMOBAR)
                (void)snprintf(colorbuf, sizeof(colorbuf), "<fc=%s>", colorstr);

        return colorbuf;
}

/*
 * Some color formats (xmobar) require to terminate colors again
 *
 */
char *endcolor() {
        if (output_format == O_XMOBAR)
                return "</fc>";
        else return "";
}

void print_seperator() {
        if (output_format == O_DZEN2)
                printf("^fg(#333333)^p(5;-2)^ro(2)^p()^fg()^p(5)");
        else if (output_format == O_XMOBAR)
                printf("<fc=#333333> | </fc>");
        else if (output_format == O_NONE)
                printf(" | ");
}
