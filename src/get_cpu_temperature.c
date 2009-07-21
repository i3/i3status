// vim:ts=8:expandtab
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#include "i3status.h"

/*
 * Reads the CPU temperature from /sys/class/thermal/thermal_zone0/temp and
 * returns the temperature in degree celcius.
 *
 */
const char *get_cpu_temperature_info() {
        static char buf[16];
        long int temp;

        slurp(thermal_zone, buf, sizeof(buf));
        temp = strtol(buf, NULL, 10);

        if (temp == LONG_MIN || temp == LONG_MAX || temp <= 0)
                (void)snprintf(buf, sizeof(buf), "T: ? C");
        else
                (void)snprintf(buf, sizeof(buf), "T: %ld C", (temp/1000));

        return buf;
}
