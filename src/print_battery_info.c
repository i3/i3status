// vim:ts=8:expandtab
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "i3status.h"

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

/*
 * Get battery information from /sys. Note that it uses the design capacity to
 * calculate the percentage, not the last full capacity, so you can see how
 * worn off your battery is.
 *
 */
void print_battery_info(int number, const char *format, bool last_full_capacity) {
        time_t empty_time;
        struct tm *empty_tm;
        char buf[1024];
        char statusbuf[16];
        char percentagebuf[16];
        char remainingbuf[256];
        char emptytimebuf[256];
        const char *walk, *last;
        int full_design = -1,
            remaining = -1,
            present_rate = -1;
        charging_status_t status = CS_DISCHARGING;

        memset(statusbuf, '\0', sizeof(statusbuf));
        memset(percentagebuf, '\0', sizeof(percentagebuf));
        memset(remainingbuf, '\0', sizeof(remainingbuf));
        memset(emptytimebuf, '\0', sizeof(emptytimebuf));

#if defined(LINUX)
        static char batpath[512];
        sprintf(batpath, "/sys/class/power_supply/BAT%d/uevent", number);
        if (!slurp(batpath, buf, sizeof(buf))) {
                printf("No battery");
                return;
        }

        for (walk = buf, last = buf; (walk-buf) < 1024; walk++) {
                if (*walk == '\n') {
                        last = walk+1;
                        continue;
                }

                if (*walk != '=')
                        continue;

                if (BEGINS_WITH(last, "POWER_SUPPLY_ENERGY_NOW") ||
                    BEGINS_WITH(last, "POWER_SUPPLY_CHARGE_NOW"))
                        remaining = atoi(walk+1);
                else if (BEGINS_WITH(last, "POWER_SUPPLY_CURRENT_NOW"))
                        present_rate = atoi(walk+1);
                else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Charging"))
                        status = CS_CHARGING;
                else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Full"))
                        status = CS_FULL;
                else {
                        /* The only thing left is the full capacity */
                        if (last_full_capacity) {
                                if (!BEGINS_WITH(last, "POWER_SUPPLY_ENERGY_FULL") &&
                                    !BEGINS_WITH(last, "POWER_SUPPLY_CHARGE_FULL"))
                                        continue;
                        } else {
                                if (!BEGINS_WITH(last, "POWER_SUPPLY_CHARGE_FULL_DESIGN") &&
                                    !BEGINS_WITH(last, "POWER_SUPPLY_ENERGY_FULL_DESIGN"))
                                        continue;
                        }

                        full_design = atoi(walk+1);
                }
        }

        if ((full_design == 1) || (remaining == -1))
                return;

        (void)snprintf(statusbuf, sizeof(statusbuf), "%s",
                        (status == CS_CHARGING ? "CHR" :
                         (status == CS_DISCHARGING ? "BAT" : "FULL")));

        (void)snprintf(percentagebuf, sizeof(percentagebuf), "%.02f%%",
                       (((float)remaining / (float)full_design) * 100));

        if (present_rate > 0) {
                float remaining_time;
                int seconds, hours, minutes, seconds_remaining;
                if (status == CS_CHARGING)
                        remaining_time = ((float)full_design - (float)remaining) / (float)present_rate;
                else if (status == CS_DISCHARGING)
                        remaining_time = ((float)remaining / (float)present_rate);
                else remaining_time = 0;

                seconds_remaining = (int)(remaining_time * 3600.0);

                hours = seconds_remaining / 3600;
                seconds = seconds_remaining - (hours * 3600);
                minutes = seconds / 60;
                seconds -= (minutes * 60);

                (void)snprintf(remainingbuf, sizeof(remainingbuf), "%02d:%02d:%02d",
                        max(hours, 0), max(minutes, 0), max(seconds, 0));
                
                empty_time = time(NULL);
                empty_time += seconds_remaining;
                empty_tm = localtime(&empty_time);

                (void)snprintf(emptytimebuf, sizeof(emptytimebuf), "%02d:%02d:%02d",
                        max(empty_tm->tm_hour, 0), max(empty_tm->tm_min, 0), max(empty_tm->tm_sec, 0));
        }
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
        int state;
        int sysctl_rslt;
        size_t sysctl_size = sizeof(sysctl_rslt);

        if (sysctlbyname(BATT_LIFE, &sysctl_rslt, &sysctl_size, NULL, 0) != 0) {
                printf("No battery");
                return;
        }

        present_rate = sysctl_rslt;
        if (sysctlbyname(BATT_TIME, &sysctl_rslt, &sysctl_size, NULL, 0) != 0) {
                printf("No battery");
                return;
        }

        remaining = sysctl_rslt;
        if (sysctlbyname(BATT_STATE, &sysctl_rslt, &sysctl_size, NULL,0) != 0) {
                printf("No battery");
                return;
        }

        state = sysctl_rslt;
        if (state == 0 && present_rate == 100)
                status = CS_FULL;
        else if (state == 0 && present_rate < 100)
                status = CS_CHARGING;
        else
                status = CS_DISCHARGING;

        full_design = sysctl_rslt;

        (void)snprintf(statusbuf, sizeof(statusbuf), "%s",
                        (status == CS_CHARGING ? "CHR" :
                         (status == CS_DISCHARGING ? "BAT" : "FULL")));

        (void)snprintf(percentagebuf, sizeof(percentagebuf), "%02d%%",
                       present_rate);

        if (state == 1) {
                int hours, minutes;
                minutes = remaining;
                hours = minutes / 60;
                minutes -= (hours * 60);
                (void)snprintf(remainingbuf, sizeof(remainingbuf), "%02dh%02d",
                               max(hours, 0), max(minutes, 0));
        }
#endif

        for (walk = format; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        putchar(*walk);
                        continue;
                }

                if (strncmp(walk+1, "status", strlen("status")) == 0) {
                        printf("%s", statusbuf);
                        walk += strlen("status");
                } else if (strncmp(walk+1, "percentage", strlen("percentage")) == 0) {
                        printf("%s", percentagebuf);
                        walk += strlen("percentage");
                } else if (strncmp(walk+1, "remaining", strlen("remaining")) == 0) {
                        printf("%s", remainingbuf);
                        walk += strlen("remaining");
                } else if (strncmp(walk+1, "emptytime", strlen("emptytime")) == 0) {
                        printf("%s", emptytimebuf);
                        walk += strlen("emptytime");
                }
        }
}
