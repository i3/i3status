// vim:ts=4:sw=4:expandtab
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <err.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(__DragonFly__)
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/sensors.h>
#endif

#if defined(__OpenBSD__)
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>
#endif

#if defined(__NetBSD__)
#include <fcntl.h>
#include <prop/proplib.h>
#include <sys/envsys.h>
#endif

#define CPUINFOBUFSIZE 32

/*
 * Reads /proc/cpuinfo and returns an average (combined) load of all CPUs.
 *
 */
void print_cpu_freq(yajl_gen json_gen, char *buffer, const char *format) {
    const char *walk;
    char *outwalk = buffer;
    char cpu_freq[CPUINFOBUFSIZE] = {0};

#if defined(LINUX)
    size_t linesize = 0;
    size_t n_cpu = 0;
    ssize_t linelen = 0;
    double mhz_ttl = 0.0;
    char *line = NULL;
    char *cpuinfo = "/proc/cpuinfo";
    FILE *fp = NULL;

    if ((fp = fopen(cpuinfo, "r")) == NULL)
        goto error;

    while ((linelen = getline(&line, &linesize, fp)) != -1) {
        if (strncmp(line, "cpu MHz", 7) == 0) {
            double MHz = 0;
            char *tmp = strrchr(line, ':');

            tmp[strcspn(tmp, "\n")] = '\0';
            for (; !isdigit(*tmp); ++tmp)
                ;

            if (sscanf(tmp, "%lf", &MHz) != 1 || MHz < 1)
                goto lnxerror;

            mhz_ttl += MHz;
            n_cpu++;
        }
    }
    (void)snprintf(cpu_freq, CPUINFOBUFSIZE, "%.0f", mhz_ttl / n_cpu);
lnxerror:
    fclose(fp);
    free(line);
#elif defined(__DragonFly__)
/* TODO To be implemented */
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
/* TODO To be implemented */
#elif defined(__OpenBSD__)
    /* Based on /usr/src/usr.sbin/apm.c code. */
    /* XXX Need to check if this is a single CPU or avg of all CPUs. */
    int cpuspeed = 0;
    int cpuspeed_mib[] = {CTL_HW, HW_CPUSPEED};
    size_t cpurspeed_sz = sizeof(cpuspeed);

    if (sysctl(cpuspeed_mib, 2, &cpuspeed, &cpurspeed_sz, NULL, 0) < 0)
        goto error;
    (void)snprintf(cpu_freq, CPUINFOBUFSIZE, "%.0f", cpuspeed);
#elif defined(__NetBSD__)
/* TODO To be implemented */
#endif
    for (walk = format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }
        if (BEGINS_WITH(walk + 1, "MHz")) {
            outwalk += sprintf(outwalk, "%s", cpu_freq);
            walk += strlen("MHz");
        }
    }

    OUTPUT_FULL_TEXT(buffer);
    return;
error:
    OUTPUT_FULL_TEXT("can't read cpu frequency");
    (void)fputs("i3status: Cannot read CPU frequency.\n", stderr);
}
