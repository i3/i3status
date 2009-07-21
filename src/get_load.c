// vim:ts=8:expandtab
#include "i3status.h"
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char *get_load() {
        static char part[512];

/* Get load */
#if defined(__FreeBSD__) || defined(linux) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(sun)
        double loadavg[3];
        if (getloadavg(loadavg, 3) == -1)
                errx(-1, "getloadavg() failed\n");
        (void)snprintf(part, sizeof(part), "%1.2f %1.2f %1.2f", loadavg[0], loadavg[1], loadavg[2]);
#else
        part[0] = '\0';
#endif

        return part;
}
