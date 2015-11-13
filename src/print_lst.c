// vim:ts=4:sw=4:expandtab
/* LST equations from https://www.cv.nrao.edu/~rfisher/Ephemerides/times.html */
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

void print_lst(yajl_gen json_gen, char *buffer, const char *title, const char *lon) {
    char *outwalk = buffer;
    time_t now;
    double d, t, GMST_s, LMST_s;
    int hour, min, sec;

    if (title != NULL)
        INSTANCE(title);

    now = time(NULL);
    d = (now / 86400.0) + 2440587.5 - 2451545.0; /* days since 2000 Jan 1 1200h UTC */
    t = d / 36525;                               /* centuries since 2000 Jan 1 1200h UTC */
    GMST_s = 24110.54841 + 8640184.812866 * t + 0.093104 * t * t - 0.0000062 * t * t * t;
    GMST_s += now;
    GMST_s = GMST_s - 86400.0 * floor(GMST_s / 86400.0);

    /* adjust to local LMST */
    LMST_s = GMST_s + 3600 * atof(lon) / 15.;

    if (LMST_s <= 0) { /* LMST is between 0 and 24h */
        LMST_s += 86400.0;
    }

    hour = floor(LMST_s / 3600.);
    LMST_s -= 3600. * floor(LMST_s / 3600);
    min = floor(LMST_s / 60.);
    sec = LMST_s - 60 * floor(LMST_s / 60.);

    outwalk += sprintf(outwalk, "LST: %02d:%02d:%02d", hour, min, sec);
    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);
}
