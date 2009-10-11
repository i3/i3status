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
#ifdef DZEN
        (void)snprintf(colorbuf, sizeof(colorbuf), "^fg(%s)", colorstr);
#elif XMOBAR
        (void)snprintf(colorbuf, sizeof(colorbuf), "<fc=%s>", colorstr);
#endif
        return colorbuf;
}

/*
 * Some color formats (xmobar) require to terminate colors again
 *
 */
char *endcolor() {
#ifdef XMOBAR
        return "</fc>";
#else
        return "";
#endif
}

void print_seperator() {
#if defined(DZEN) || defined(XMOBAR)
        printf("%s", BAR);
#endif
}
