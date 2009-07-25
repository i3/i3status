// vim:ts=8:expandtab
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#include "i3status.h"

#if defined(__FreeBSD__)
#include <err.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#define TZ_ZEROC 2732
#define TZ_KELVTOC(x) (((x) - TZ_ZEROC) / 10), abs(((x) - TZ_ZEROC) % 10)
#endif


/*
 * Reads the CPU temperature from /sys/class/thermal/thermal_zone0/temp and
 * returns the temperature in degree celcius.
 *
 */
const char *get_cpu_temperature_info() {
        static char buf[16];

#if defined(LINUX)
        long int temp;
        if (!slurp(thermal_zone, buf, sizeof(buf)))
                die("Could not open \"%s\"\n", thermal_zone);
        temp = strtol(buf, NULL, 10);
        if (temp == LONG_MIN || temp == LONG_MAX || temp <= 0)
                (void)snprintf(buf, sizeof(buf), "T: ? C");
        else
                (void)snprintf(buf, sizeof(buf), "T: %ld C", (temp/1000));
#elif defined(__FreeBSD__)
        int sysctl_rslt;
        size_t sysctl_size = sizeof (sysctl_rslt);
        if (sysctlbyname(thermal_zone,&sysctl_rslt,&sysctl_size,NULL,0))
            return "No Thermal";

        snprintf(buf,sizeof(buf),"T: %d.%d C",TZ_KELVTOC(sysctl_rslt));
#endif

        return buf;
}
