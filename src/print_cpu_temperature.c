// vim:ts=8:expandtab
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <err.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#define TZ_ZEROC 2732
#define TZ_KELVTOC(x) (((x) - TZ_ZEROC) / 10), abs(((x) - TZ_ZEROC) % 10)
#endif

#if defined(__OpenBSD__)
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>
#include <errno.h>
#include <err.h>
#endif

static char *thermal_zone;

/*
 * Reads the CPU temperature from /sys/class/thermal/thermal_zone0/temp and
 * returns the temperature in degree celcius.
 *
 */
void print_cpu_temperature_info(yajl_gen json_gen, char *buffer, int zone, const char *path, const char *format) {
#ifdef THERMAL_ZONE
        const char *walk;
        char *outwalk = buffer;
        static char buf[16];

        if (path == NULL) {
                asprintf(&thermal_zone, THERMAL_ZONE, zone);
                path = thermal_zone;
        }

        INSTANCE(path);

        for (walk = format; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        *(outwalk++) = *walk;
                        continue;
                }

                if (BEGINS_WITH(walk+1, "degrees")) {
#if defined(LINUX)
                        long int temp;
                        if (!slurp(path, buf, sizeof(buf)))
                                goto error;
                        temp = strtol(buf, NULL, 10);
                        if (temp == LONG_MIN || temp == LONG_MAX || temp <= 0)
                                *(outwalk++) = '?';
                        else
                                outwalk += sprintf(outwalk, "%ld", (temp/1000));
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
                        int sysctl_rslt;
                        size_t sysctl_size = sizeof(sysctl_rslt);
                        if (sysctlbyname(path, &sysctl_rslt, &sysctl_size, NULL, 0))
                                goto error;

                        outwalk += sprintf(outwalk, "%d.%d", TZ_KELVTOC(sysctl_rslt));
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
		/*
		 * 'path' is actually the node within the full path (eg, cpu0).
		 * XXX: Extend the API to allow a string instead of just an int for path, this would
		 * allow us to have a path of 'acpitz0' for example.
		 */
		if (strncmp(sensordev.xname, path, strlen(path)) == 0) {
			mib[3] = SENSOR_TEMP;
			for (numt = 0; numt < sensordev.maxnumt[SENSOR_TEMP]; numt++) {
				mib[4] = numt;
				if (sysctl(mib, 5, &sensor, &slen, NULL, 0) == -1) {
					if (errno != ENOENT)
						warn("sysctl");
					continue;
				}
				outwalk += sprintf(outwalk, "%.2f", (sensor.value - 273150000) / 1000000.0 );
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
        (void)fputs("Cannot read temperature\n", stderr);
}
