// vim:ts=8:expandtab
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(__OpenBSD__)
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <machine/apmvar.h>
#endif

#define BATT_STATUS_NAME(status) \
    (status == CS_CHARGING ? "CHR" : \
        (status == CS_DISCHARGING ? "BAT" : "FULL"))
/*
 * Get battery information from /sys. Note that it uses the design capacity to
 * calculate the percentage, not the last full capacity, so you can see how
 * worn off your battery is.
 *
 */
void print_battery_info(yajl_gen json_gen, char *buffer, int number, const char *path, const char *format, const char *format_down, int low_threshold, char *threshold_type, bool last_full_capacity, bool integer_battery_capacity, bool hide_seconds) {
        time_t empty_time;
        struct tm *empty_tm;
        char buf[1024];
        char statusbuf[16];
        char percentagebuf[16];
        char remainingbuf[256];
        char emptytimebuf[256];
        char consumptionbuf[256];
        const char *walk, *last;
        char *outwalk = buffer;
        bool watt_as_unit;
        bool colorful_output = false;
        int full_design = -1,
            remaining = -1,
            present_rate = -1,
            voltage = -1;
        charging_status_t status = CS_DISCHARGING;

        memset(statusbuf, '\0', sizeof(statusbuf));
        memset(percentagebuf, '\0', sizeof(percentagebuf));
        memset(remainingbuf, '\0', sizeof(remainingbuf));
        memset(emptytimebuf, '\0', sizeof(emptytimebuf));
        memset(consumptionbuf, '\0', sizeof(consumptionbuf));

        static char batpath[512];
        sprintf(batpath, path, number);
        INSTANCE(batpath);

#if defined(LINUX)
        if (!slurp(batpath, buf, sizeof(buf))) {
                OUTPUT_FULL_TEXT(format_down);
                return;
        }

        for (walk = buf, last = buf; (walk-buf) < 1024; walk++) {
                if (*walk == '\n') {
                        last = walk+1;
                        continue;
                }

                if (*walk != '=')
                        continue;

                if (BEGINS_WITH(last, "POWER_SUPPLY_ENERGY_NOW")) {
                        watt_as_unit = true;
                        remaining = atoi(walk+1);
                }
                else if (BEGINS_WITH(last, "POWER_SUPPLY_CHARGE_NOW")) {
                        watt_as_unit = false;
                        remaining = atoi(walk+1);
                }
                else if (BEGINS_WITH(last, "POWER_SUPPLY_CURRENT_NOW"))
                        present_rate = atoi(walk+1);
                else if (BEGINS_WITH(last, "POWER_SUPPLY_VOLTAGE_NOW"))
                        voltage = atoi(walk+1);
                /* on some systems POWER_SUPPLY_POWER_NOW does not exist, but actually
                 * it is the same as POWER_SUPPLY_CURRENT_NOW but with μWh as
                 * unit instead of μAh. We will calculate it as we need it
                 * later. */
                else if (BEGINS_WITH(last, "POWER_SUPPLY_POWER_NOW"))
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

        /* the difference between POWER_SUPPLY_ENERGY_NOW and
         * POWER_SUPPLY_CHARGE_NOW is the unit of measurement. The energy is
         * given in mWh, the charge in mAh. So calculate every value given in
         * ampere to watt */
        if (!watt_as_unit) {
            present_rate = (((float)voltage / 1000.0) * ((float)present_rate / 1000.0));
            remaining = (((float)voltage / 1000.0) * ((float)remaining / 1000.0));
            full_design = (((float)voltage / 1000.0) * ((float)full_design / 1000.0));
        }

        if ((full_design == -1) || (remaining == -1)) {
                OUTPUT_FULL_TEXT(format_down);
                return;
        }

        (void)snprintf(statusbuf, sizeof(statusbuf), "%s", BATT_STATUS_NAME(status));

        float percentage_remaining = (((float)remaining / (float)full_design) * 100);
        if (integer_battery_capacity) {
                (void)snprintf(percentagebuf, sizeof(percentagebuf), "%.00f%%", percentage_remaining);
        } else {
                (void)snprintf(percentagebuf, sizeof(percentagebuf), "%.02f%%", percentage_remaining);
        }

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

                if (status == CS_DISCHARGING && low_threshold > 0) {
                        if (strncmp(threshold_type, "percentage", strlen(threshold_type)) == 0
                                && percentage_remaining < low_threshold) {
                                START_COLOR("color_bad");
                                colorful_output = true;
                        } else if (strncmp(threshold_type, "time", strlen(threshold_type)) == 0
                                && seconds_remaining < 60 * low_threshold) {
                                START_COLOR("color_bad");
                                colorful_output = true;
                        } else {
                            colorful_output = false;
                        }
                }

                if (hide_seconds)
                        (void)snprintf(remainingbuf, sizeof(remainingbuf), "%02d:%02d",
                                max(hours, 0), max(minutes, 0));
                else
                        (void)snprintf(remainingbuf, sizeof(remainingbuf), "%02d:%02d:%02d",
                                max(hours, 0), max(minutes, 0), max(seconds, 0));

                empty_time = time(NULL);
                empty_time += seconds_remaining;
                empty_tm = localtime(&empty_time);

                if (hide_seconds)
                        (void)snprintf(emptytimebuf, sizeof(emptytimebuf), "%02d:%02d",
                                max(empty_tm->tm_hour, 0), max(empty_tm->tm_min, 0));
                else
                        (void)snprintf(emptytimebuf, sizeof(emptytimebuf), "%02d:%02d:%02d",
                                max(empty_tm->tm_hour, 0), max(empty_tm->tm_min, 0), max(empty_tm->tm_sec, 0));

                (void)snprintf(consumptionbuf, sizeof(consumptionbuf), "%1.2fW",
                        ((float)present_rate / 1000.0 / 1000.0));
        } else {
                /* On some systems, present_rate may not exist. Still, make sure
                 * we colorize the output if threshold_type is set to percentage
                 * (since we don't have any information on remaining time). */
                if (status == CS_DISCHARGING && low_threshold > 0) {
                        if (strncmp(threshold_type, "percentage", strlen(threshold_type)) == 0
                                && percentage_remaining < low_threshold) {
                                START_COLOR("color_bad");
                                colorful_output = true;
                        }
                }
        }
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
        int state;
        int sysctl_rslt;
        size_t sysctl_size = sizeof(sysctl_rslt);

        if (sysctlbyname(BATT_LIFE, &sysctl_rslt, &sysctl_size, NULL, 0) != 0) {
                OUTPUT_FULL_TEXT(format_down);
                return;
        }

        present_rate = sysctl_rslt;
        if (sysctlbyname(BATT_TIME, &sysctl_rslt, &sysctl_size, NULL, 0) != 0) {
                OUTPUT_FULL_TEXT(format_down);
                return;
        }

        remaining = sysctl_rslt;
        if (sysctlbyname(BATT_STATE, &sysctl_rslt, &sysctl_size, NULL,0) != 0) {
                OUTPUT_FULL_TEXT(format_down);
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

        (void)snprintf(statusbuf, sizeof(statusbuf), "%s", BATT_STATUS_NAME(status));

        (void)snprintf(percentagebuf, sizeof(percentagebuf), "%02d%%",
                       present_rate);

        if (state == 1) {
                int hours, minutes;
                minutes = remaining;
                hours = minutes / 60;
                minutes -= (hours * 60);
                (void)snprintf(remainingbuf, sizeof(remainingbuf), "%02dh%02d",
                               max(hours, 0), max(minutes, 0));
		if (strncmp(threshold_type, "percentage", strlen(threshold_type)) == 0
		    && present_rate < low_threshold) {
			START_COLOR("color_bad");
			colorful_output = true;
		} else if (strncmp(threshold_type, "time", strlen(threshold_type)) == 0
			   && remaining < (u_int) low_threshold) {
			START_COLOR("color_bad");
			colorful_output = true;
		}
        }
#elif defined(__OpenBSD__)
	/*
	 * We're using apm(4) here, which is the interface to acpi(4) on amd64/i386 and
	 * the generic interface on macppc/sparc64/zaurus, instead of using sysctl(3) and
	 * probing acpi(4) devices.
	 */
	struct apm_power_info apm_info;
	int apm_fd;

	apm_fd = open("/dev/apm", O_RDONLY);
	if (apm_fd < 0) {
		OUTPUT_FULL_TEXT("can't open /dev/apm");
		return;
	}
	if (ioctl(apm_fd, APM_IOC_GETPOWER, &apm_info) < 0)
		OUTPUT_FULL_TEXT("can't read power info");

	close(apm_fd);

	/* Don't bother to go further if there's no battery present. */
	if ((apm_info.battery_state == APM_BATTERY_ABSENT) ||
	    (apm_info.battery_state == APM_BATT_UNKNOWN)) {
		OUTPUT_FULL_TEXT(format_down);
		return;
	}

	switch(apm_info.ac_state) {
	case APM_AC_OFF:
		status = CS_DISCHARGING;
		break;
	case APM_AC_ON:
		status = CS_CHARGING;
		break;
	default:
		/* If we don't know what's going on, just assume we're discharging. */
		status = CS_DISCHARGING;
		break;
	}

	(void)snprintf(statusbuf, sizeof(statusbuf), "%s", BATT_STATUS_NAME(status));
	/* integer_battery_capacity is implied as battery_life is already in whole numbers. */
	(void)snprintf(percentagebuf, sizeof(percentagebuf), "%.00d%%", apm_info.battery_life);

	if (status == CS_DISCHARGING && low_threshold > 0) {
		if (strncmp(threshold_type, "percentage", strlen(threshold_type)) == 0
		    && apm_info.battery_life < low_threshold) {
			START_COLOR("color_bad");
			colorful_output = true;
		} else if (strncmp(threshold_type, "time", strlen(threshold_type)) == 0
			   && apm_info.minutes_left < (u_int) low_threshold) {
			START_COLOR("color_bad");
			colorful_output = true;
		}
	}

	/* Can't give a meaningful value for remaining minutes if we're charging. */
	if (status != CS_CHARGING) {
		(void)snprintf(remainingbuf, sizeof(remainingbuf), "%d", apm_info.minutes_left);
	} else {
		(void)snprintf(remainingbuf, sizeof(remainingbuf), "%s", "(CHR)");
	}

	if (colorful_output)
		END_COLOR;
#endif

#define EAT_SPACE_FROM_OUTPUT_IF_EMPTY(_buf) \
        do { \
                if (strlen(_buf) == 0) { \
                        if (outwalk > buffer && isspace(outwalk[-1])) \
                                outwalk--; \
                        else if (isspace(*(walk+1))) \
                                walk++; \
                } \
        } while (0)

        for (walk = format; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        *(outwalk++) = *walk;
                        continue;
                }

                if (strncmp(walk+1, "status", strlen("status")) == 0) {
                        outwalk += sprintf(outwalk, "%s", statusbuf);
                        walk += strlen("status");
                } else if (strncmp(walk+1, "percentage", strlen("percentage")) == 0) {
                        outwalk += sprintf(outwalk, "%s", percentagebuf);
                        walk += strlen("percentage");
                } else if (strncmp(walk+1, "remaining", strlen("remaining")) == 0) {
                        outwalk += sprintf(outwalk, "%s", remainingbuf);
                        walk += strlen("remaining");
                        EAT_SPACE_FROM_OUTPUT_IF_EMPTY(remainingbuf);
                } else if (strncmp(walk+1, "emptytime", strlen("emptytime")) == 0) {
                        outwalk += sprintf(outwalk, "%s", emptytimebuf);
                        walk += strlen("emptytime");
                        EAT_SPACE_FROM_OUTPUT_IF_EMPTY(emptytimebuf);
                } else if (strncmp(walk+1, "consumption", strlen("consumption")) == 0) {
                        outwalk += sprintf(outwalk, "%s", consumptionbuf);
                        walk += strlen("consumption");
                        EAT_SPACE_FROM_OUTPUT_IF_EMPTY(consumptionbuf);
                }
        }

        if (colorful_output)
                END_COLOR;

        OUTPUT_FULL_TEXT(buffer);
}
