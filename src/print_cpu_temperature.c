// vim:ts=8:expandtab
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#include <err.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#define TZ_ZEROC 2732
#define TZ_KELVTOC(x) (((x) - TZ_ZEROC) / 10), abs(((x) - TZ_ZEROC) % 10)
#define TZ_AVG(x) ((x) - TZ_ZEROC) / 10
#endif

#if defined(__OpenBSD__)
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>
#include <errno.h>
#include <err.h>

#define MUKTOC(v) ((v - 273150000) / 1000000.0)
#endif

static char *thermal_zone;

/*
 * Reads the CPU temperature from /sys/class/thermal/thermal_zone0/temp and
 * returns the temperature in degree celcius.
 *
 */
void print_cpu_temperature_info(yajl_gen json_gen, char *buffer, int zone, const char *path, const char *format, int max_threshold) {
        char *outwalk = buffer;
#ifdef THERMAL_ZONE
        const char *walk;
        bool colorful_output = false;

        if (path == NULL)
                asprintf(&thermal_zone, THERMAL_ZONE, zone);
        else
                asprintf(&thermal_zone, path, zone);
        path = thermal_zone;

        INSTANCE(path);

        for (walk = format; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        *(outwalk++) = *walk;
                        continue;
                }

                if (BEGINS_WITH(walk+1, "degrees")) {
#if defined(LINUX)
                        static char buf[16];
                        long int temp;
                        if (!slurp(path, buf, sizeof(buf)))
                                goto error;
                        temp = strtol(buf, NULL, 10);
                        if (temp == LONG_MIN || temp == LONG_MAX || temp <= 0)
                                *(outwalk++) = '?';
                        else {
                                if ((temp/1000) >= max_threshold) {
                                        START_COLOR("color_bad");
                                        colorful_output = true;
                                }
                                outwalk += sprintf(outwalk, "%ld", (temp/1000));
                                if (colorful_output) {
                                        END_COLOR;
                                        colorful_output = false;
                                }
                        }
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
                        int sysctl_rslt;
                        size_t sysctl_size = sizeof(sysctl_rslt);
                        if (sysctlbyname(path, &sysctl_rslt, &sysctl_size, NULL, 0))
                                goto error;

                        if (TZ_AVG(sysctl_rslt) >= max_threshold) {
                                START_COLOR("color_bad");
                                colorful_output = true;
                        }
                        outwalk += sprintf(outwalk, "%d.%d", TZ_KELVTOC(sysctl_rslt));
                        if (colorful_output) {
                                END_COLOR;
                                colorful_output = false;
                        }

#elif defined(__OpenBSD__)
        struct sensordev sensordev;
        struct sensor sensor;
        size_t sdlen, slen;
        int dev, numt, mib[5] = { CTL_HW, HW_SENSORS, 0, 0, 0 };

        sdlen = sizeof(sensordev);
        slen = sizeof(sensor);

        for (dev = 0; ; dev++) {
                mib[2] = dev;
                if (sysctl(mib, 3, &sensordev, &sdlen, NULL, 0) == -1) {
                        if (errno == ENXIO)
                                continue;
                        if (errno == ENOENT)
                                break;
                        goto error;
                }
                /* 'path' is the node within the full path (defaults to acpitz0). */
                if (strncmp(sensordev.xname, path, strlen(path)) == 0) {
                        mib[3] = SENSOR_TEMP;
                        /* Limit to temo0, but should retrieve from a full path... */
                        for (numt = 0; numt < 1 /*sensordev.maxnumt[SENSOR_TEMP]*/; numt++) {
                                mib[4] = numt;
                                if (sysctl(mib, 5, &sensor, &slen, NULL, 0) == -1) {
                                        if (errno != ENOENT) {
                                                warn("sysctl");
                                                continue;
                                        }
                                }
                                if ((int)MUKTOC(sensor.value) >= max_threshold) {
                                        START_COLOR("color_bad");
                                        colorful_output = true;
                                }

                                outwalk += sprintf(outwalk, "%.2f", MUKTOC(sensor.value));

                                if (colorful_output) {
                                        END_COLOR;
                                        colorful_output = false;
                                }
                        }
                }
        }
#endif
                        walk += strlen("degrees");
                }
        }
        OUTPUT_FULL_TEXT(buffer);
        return;
error:
#endif
        OUTPUT_FULL_TEXT("cant read temp");
        (void)fputs("i3status: Cannot read temperature. Verify that you have a thermal zone in /sys/class/thermal or disable the cpu_temperature module in your i3status config.\n", stderr);
}
