// vim:ts=4:sw=4:expandtab
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
 * Returns the correct color format for dzen (^fg(color)), xmobar (<fc=color>)
 * or lemonbar (%{Fcolor})
 *
 */
char *color(const char *colorstr) {
    static char colorbuf[32];
    if (!cfg_getbool(cfg_general, "colors")) {
        colorbuf[0] = '\0';
        return colorbuf;
    }
    if (output_format == O_DZEN2)
        (void)snprintf(colorbuf, sizeof(colorbuf), "^fg(%s)", cfg_getstr(cfg_general, colorstr));
    else if (output_format == O_XMOBAR)
        (void)snprintf(colorbuf, sizeof(colorbuf), "<fc=%s>", cfg_getstr(cfg_general, colorstr));
    else if (output_format == O_LEMONBAR)
        (void)snprintf(colorbuf, sizeof(colorbuf), "%%{F%s}", cfg_getstr(cfg_general, colorstr));
    else if (output_format == O_TERM) {
        /* The escape-sequence for color is <CSI><col>;1m (bright/bold
         * output), where col is a 3-bit rgb-value with b in the
         * least-significant bit. We round the given color to the
         * nearist 3-bit-depth color and output the escape-sequence */
        char *str = cfg_getstr(cfg_general, colorstr);
        int col = strtol(str + 1, NULL, 16);
        int r = (col & (0xFF << 0)) / 0x80;
        int g = (col & (0xFF << 8)) / 0x8000;
        int b = (col & (0xFF << 16)) / 0x800000;
        col = (r << 2) | (g << 1) | b;
        (void)snprintf(colorbuf, sizeof(colorbuf), "\033[3%d;1m", col);
    }
    return colorbuf;
}

/*
 * Some color formats (xmobar) require to terminate colors again
 *
 */
char *endcolor(void) {
    if (output_format == O_XMOBAR)
        return "</fc>";
    else if (output_format == O_TERM)
        return "\033[0m";
    else
        return "";
}

void print_separator(const char *separator) {
    if (output_format == O_I3BAR || strlen(separator) == 0)
        return;

    if (output_format == O_DZEN2)
        printf("^fg(%s)%s^fg()", cfg_getstr(cfg_general, "color_separator"), separator);
    else if (output_format == O_XMOBAR)
        printf("<fc=%s>%s</fc>", cfg_getstr(cfg_general, "color_separator"), separator);
    else if (output_format == O_LEMONBAR)
        printf("%%{F%s}%s%%{F-}", cfg_getstr(cfg_general, "color_separator"), separator);
    else if (output_format == O_TERM)
        printf("%s%s%s", color("color_separator"), separator, endcolor());
    else if (output_format == O_NONE)
        printf("%s", separator);
}

/*
 * The term-output hides the cursor. We call this on exit to reset that.
 */
void reset_cursor(void) {
    printf("\033[?25h");
}
