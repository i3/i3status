// vim:ts=4:sw=4:expandtab
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#define BACKLIGHT_CUR_BRIGHTNESS "%s/brightness"
#define BACKLIGHT_MAX_BRIGHTNESS "%s/max_brightness"
#define ERROR_CODE ULONG_MAX

static unsigned long slurp_positive_int(const char *filename) {
    char buf[16];

    if (!slurp(filename, buf, sizeof(buf)))
        return ERROR_CODE;

    return strtoul(buf, NULL, 10);
}

static unsigned long slurp_cur_brightness(const char *device) {
    char fnbuff[256];
    snprintf(fnbuff, sizeof fnbuff, BACKLIGHT_CUR_BRIGHTNESS, device);
    return slurp_positive_int(fnbuff);
}

static int slurp_max_brightness(const char *device) {
    char fnbuff[256];
    snprintf(fnbuff, sizeof fnbuff, BACKLIGHT_MAX_BRIGHTNESS, device);
    return slurp_positive_int(fnbuff);
}

void print_backlight(yajl_gen json_gen, char *buffer, const char *fmt, const char *device) {
    const char *walk;
    char *outwalk = buffer;
    unsigned long cur_brightness, max_brightness, brightness;

    INSTANCE("backlight");

    if ((cur_brightness = slurp_cur_brightness(device)) == ERROR_CODE)
        goto error;
    if ((max_brightness = slurp_max_brightness(device)) == ERROR_CODE)
        goto error;

    brightness = (int)(((double)cur_brightness * 100) / (double)max_brightness);

    for (walk = fmt; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "percentage")) {
            outwalk += sprintf(outwalk, "%.2lu%s", brightness, pct_mark);
            walk += strlen("percentage");
        }
    }

    OUTPUT_FULL_TEXT(buffer);
    return;
error:
    OUTPUT_FULL_TEXT("can't read backlight info");
    (void)fputs("i3status: Cannot read backlight information. Verify that you have something in /sys/class/backlight or disable the backlight module in your i3status config.\n", stderr);
}
