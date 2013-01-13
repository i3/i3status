// vim:ts=8:expandtab
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

static int local_timezone_init = 0;
static const char *local_timezone = NULL;
static const char *current_timezone = NULL;

void set_timezone(const char *timezone) {
        if (!local_timezone_init) {
                /* First call, initialize. */
                local_timezone = getenv("TZ");
                local_timezone_init = 1;
        }
        if (timezone == NULL || timezone[0] == '\0') {
                /* User wants localtime. */
                timezone = local_timezone;
        }
        if (timezone != current_timezone) {
                if (timezone) {
                        setenv("TZ", timezone, 1);
                } else {
                        unsetenv("TZ");
                }
                tzset();
                current_timezone = timezone;
        }
}

void print_time(yajl_gen json_gen, char *buffer, const char *format, const char *timezone, time_t t) {
        char *outwalk = buffer;
        struct tm tm;

        /* Convert time and format output. */
        set_timezone(timezone);
        localtime_r(&t, &tm);
        outwalk += strftime(outwalk, 4095, format, &tm);
        *outwalk = '\0';
        OUTPUT_FULL_TEXT(buffer);
}
