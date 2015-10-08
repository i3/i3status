// vim:ts=4:sw=4:expandtab
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
#include <dev/acpica/acpiio.h>
#endif

#if defined(__OpenBSD__)
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <machine/apmvar.h>
#endif

#if defined(__NetBSD__)
#include <fcntl.h>
#include <prop/proplib.h>
#include <sys/envsys.h>
#endif

/*
 * Get battery information from /sys. Note that it uses the design capacity to
 * calculate the percentage, not the last full capacity, so you can see how
 * worn off your battery is.
 *
 */
void print_battery_info(yajl_gen json_gen, char *buffer, int number, const char *path, const char *format, const char *format_down, const char *status_chr, const char *status_bat, const char *status_full, int low_threshold, char *threshold_type, bool last_full_capacity, bool integer_battery_capacity, bool hide_seconds) {
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
    bool watt_as_unit = false;
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

#define BATT_STATUS_NAME(status) \
    (status == CS_CHARGING ? status_chr : (status == CS_DISCHARGING ? status_bat : status_full))

#if defined(LINUX)
    if (!slurp(batpath, buf, sizeof(buf))) {
        OUTPUT_FULL_TEXT(format_down);
        return;
    }

    for (walk = buf, last = buf; (walk - buf) < 1024; walk++) {
        if (*walk == '\n') {
            last = walk + 1;
            continue;
        }

        if (*walk != '=')
            continue;

        if (BEGINS_WITH(last, "POWER_SUPPLY_ENERGY_NOW")) {
            watt_as_unit = true;
            remaining = atoi(walk + 1);
        } else if (BEGINS_WITH(last, "POWER_SUPPLY_CHARGE_NOW")) {
            watt_as_unit = false;
            remaining = atoi(walk + 1);
        } else if (BEGINS_WITH(last, "POWER_SUPPLY_CURRENT_NOW"))
            present_rate = abs(atoi(walk + 1));
        else if (BEGINS_WITH(last, "POWER_SUPPLY_VOLTAGE_NOW"))
            voltage = abs(atoi(walk + 1));
        /* on some systems POWER_SUPPLY_POWER_NOW does not exist, but actually
         * it is the same as POWER_SUPPLY_CURRENT_NOW but with μWh as
         * unit instead of μAh. We will calculate it as we need it
         * later. */
        else if (BEGINS_WITH(last, "POWER_SUPPLY_POWER_NOW"))
            present_rate = abs(atoi(walk + 1));
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

            full_design = atoi(walk + 1);
        }
    }

    /* the difference between POWER_SUPPLY_ENERGY_NOW and
     * POWER_SUPPLY_CHARGE_NOW is the unit of measurement. The energy is
     * given in mWh, the charge in mAh. So calculate every value given in
     * ampere to watt */
    if (!watt_as_unit) {
        present_rate = (((float)voltage / 1000.0) * ((float)present_rate / 1000.0));

        if (voltage != -1) {
            remaining = (((float)voltage / 1000.0) * ((float)remaining / 1000.0));
            full_design = (((float)voltage / 1000.0) * ((float)full_design / 1000.0));
        }
    }

    if ((full_design == -1) || (remaining == -1)) {
        OUTPUT_FULL_TEXT(format_down);
        return;
    }

    (void)snprintf(statusbuf, sizeof(statusbuf), "%s", BATT_STATUS_NAME(status));

    float percentage_remaining = (((float)remaining / (float)full_design) * 100);
    /* Some batteries report POWER_SUPPLY_CHARGE_NOW=<full_design> when fully
     * charged, even though that’s plainly wrong. For people who chose to see
     * the percentage calculated based on the last full capacity, we clamp the
     * value to 100%, as that makes more sense.
     * See http://bugs.debian.org/785398 */
    if (last_full_capacity && percentage_remaining > 100) {
        percentage_remaining = 100;
    }
    if (integer_battery_capacity) {
        (void)snprintf(percentagebuf, sizeof(percentagebuf), "%.00f%s", percentage_remaining, pct_mark);
    } else {
        (void)snprintf(percentagebuf, sizeof(percentagebuf), "%.02f%s", percentage_remaining, pct_mark);
    }

    if (present_rate > 0) {
        float remaining_time;
        int seconds, hours, minutes, seconds_remaining;
        if (status == CS_CHARGING)
            remaining_time = ((float)full_design - (float)remaining) / (float)present_rate;
        else if (status == CS_DISCHARGING)
            remaining_time = ((float)remaining / (float)present_rate);
        else
            remaining_time = 0;

        seconds_remaining = (int)(remaining_time * 3600.0);

        hours = seconds_remaining / 3600;
        seconds = seconds_remaining - (hours * 3600);
        minutes = seconds / 60;
        seconds -= (minutes * 60);

        if (status == CS_DISCHARGING && low_threshold > 0) {
            if (strcasecmp(threshold_type, "percentage") == 0 && percentage_remaining < low_threshold) {
                START_COLOR("color_bad");
                colorful_output = true;
            } else if (strcasecmp(threshold_type, "time") == 0 && seconds_remaining < 60 * low_threshold) {
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
            if (strcasecmp(threshold_type, "percentage") == 0 && percentage_remaining < low_threshold) {
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
    if (sysctlbyname(BATT_STATE, &sysctl_rslt, &sysctl_size, NULL, 0) != 0) {
        OUTPUT_FULL_TEXT(format_down);
        return;
    }

    state = sysctl_rslt;
    if (state == 0 && present_rate == 100)
        status = CS_FULL;
    else if ((state & ACPI_BATT_STAT_CHARGING) && present_rate < 100)
        status = CS_CHARGING;
    else
        status = CS_DISCHARGING;

    full_design = sysctl_rslt;

    (void)snprintf(statusbuf, sizeof(statusbuf), "%s", BATT_STATUS_NAME(status));

    (void)snprintf(percentagebuf, sizeof(percentagebuf), "%02d%s",
                   present_rate, pct_mark);

    if (state == ACPI_BATT_STAT_DISCHARG) {
        int hours, minutes;
        minutes = remaining;
        hours = minutes / 60;
        minutes -= (hours * 60);
        (void)snprintf(remainingbuf, sizeof(remainingbuf), "%02d:%02d",
                       max(hours, 0), max(minutes, 0));
        if (strcasecmp(threshold_type, "percentage") == 0 && present_rate < low_threshold) {
            START_COLOR("color_bad");
            colorful_output = true;
        } else if (strcasecmp(threshold_type, "time") == 0 && remaining < (u_int)low_threshold) {
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

    switch (apm_info.ac_state) {
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
    (void)snprintf(percentagebuf, sizeof(percentagebuf), "%.00d%s", apm_info.battery_life, pct_mark);

    if (status == CS_DISCHARGING && low_threshold > 0) {
        if (strcasecmp(threshold_type, "percentage") == 0 && apm_info.battery_life < low_threshold) {
            START_COLOR("color_bad");
            colorful_output = true;
        } else if (strcasecmp(threshold_type, "time") == 0 && apm_info.minutes_left < (u_int)low_threshold) {
            START_COLOR("color_bad");
            colorful_output = true;
        }
    }

    /* Can't give a meaningful value for remaining minutes if we're charging. */
    if (status != CS_CHARGING) {
        (void)snprintf(remainingbuf, sizeof(remainingbuf), "%02d:%02d", apm_info.minutes_left / 60, apm_info.minutes_left % 60);
    } else {
        (void)snprintf(remainingbuf, sizeof(remainingbuf), "%s", "(CHR)");
    }

    if (colorful_output)
        END_COLOR;
#elif defined(__NetBSD__)
    /*
     * Using envsys(4) via sysmon(4).
     */
    int fd, rval, last_full_cap;
    bool is_found = false;
    char *sensor_desc;
    bool is_full = false;

    prop_dictionary_t dict;
    prop_array_t array;
    prop_object_iterator_t iter;
    prop_object_iterator_t iter2;
    prop_object_t obj, obj2, obj3, obj4, obj5;

    asprintf(&sensor_desc, "acpibat%d", number);

    fd = open("/dev/sysmon", O_RDONLY);
    if (fd < 0) {
        OUTPUT_FULL_TEXT("can't open /dev/sysmon");
        return;
    }

    rval = prop_dictionary_recv_ioctl(fd, ENVSYS_GETDICTIONARY, &dict);
    if (rval == -1) {
        close(fd);
        return;
    }

    if (prop_dictionary_count(dict) == 0) {
        prop_object_release(dict);
        close(fd);
        return;
    }

    iter = prop_dictionary_iterator(dict);
    if (iter == NULL) {
        prop_object_release(dict);
        close(fd);
    }

    /* iterate over the dictionary returned by the kernel */
    while ((obj = prop_object_iterator_next(iter)) != NULL) {
        /* skip this dict if it's not what we're looking for */
        if ((strlen(prop_dictionary_keysym_cstring_nocopy(obj)) == strlen(sensor_desc)) &&
            (strncmp(sensor_desc,
                     prop_dictionary_keysym_cstring_nocopy(obj),
                     strlen(sensor_desc)) != 0))
            continue;

        is_found = true;

        array = prop_dictionary_get_keysym(dict, obj);
        if (prop_object_type(array) != PROP_TYPE_ARRAY) {
            prop_object_iterator_release(iter);
            prop_object_release(dict);
            close(fd);
            return;
        }

        iter2 = prop_array_iterator(array);
        if (!iter2) {
            prop_object_iterator_release(iter);
            prop_object_release(dict);
            close(fd);
            return;
        }

        /* iterate over array of dicts specific to target battery */
        while ((obj2 = prop_object_iterator_next(iter2)) != NULL) {
            obj3 = prop_dictionary_get(obj2, "description");

            if (obj3 &&
                strlen(prop_string_cstring_nocopy(obj3)) == 8 &&
                strncmp("charging",
                        prop_string_cstring_nocopy(obj3),
                        8) == 0) {
                obj3 = prop_dictionary_get(obj2, "cur-value");

                if (prop_number_integer_value(obj3))
                    status = CS_CHARGING;
                else
                    status = CS_DISCHARGING;

                continue;
            }

            if (obj3 &&
                strlen(prop_string_cstring_nocopy(obj3)) == 6 &&
                strncmp("charge",
                        prop_string_cstring_nocopy(obj3),
                        6) == 0) {
                obj3 = prop_dictionary_get(obj2, "cur-value");
                obj4 = prop_dictionary_get(obj2, "max-value");
                obj5 = prop_dictionary_get(obj2, "type");

                remaining = prop_number_integer_value(obj3);
                full_design = prop_number_integer_value(obj4);

                if (remaining == full_design)
                    is_full = true;

                if (strncmp("Ampere hour",
                            prop_string_cstring_nocopy(obj5),
                            11) == 0)
                    watt_as_unit = false;
                else
                    watt_as_unit = true;

                continue;
            }

            if (obj3 &&
                strlen(prop_string_cstring_nocopy(obj3)) == 14 &&
                strncmp("discharge rate",
                        prop_string_cstring_nocopy(obj3),
                        14) == 0) {
                obj3 = prop_dictionary_get(obj2, "cur-value");
                present_rate = prop_number_integer_value(obj3);
                continue;
            }

            if (obj3 &&
                strlen(prop_string_cstring_nocopy(obj3)) == 13 &&
                strncmp("last full cap",
                        prop_string_cstring_nocopy(obj3),
                        13) == 0) {
                obj3 = prop_dictionary_get(obj2, "cur-value");
                last_full_cap = prop_number_integer_value(obj3);
                continue;
            }

            if (obj3 &&
                strlen(prop_string_cstring_nocopy(obj3)) == 7 &&
                strncmp("voltage",
                        prop_string_cstring_nocopy(obj3),
                        7) == 0) {
                obj3 = prop_dictionary_get(obj2, "cur-value");
                voltage = prop_number_integer_value(obj3);
                continue;
            }
        }
        prop_object_iterator_release(iter2);
    }

    prop_object_iterator_release(iter);
    prop_object_release(dict);
    close(fd);

    if (!is_found) {
        OUTPUT_FULL_TEXT(format_down);
        return;
    }

    if (last_full_capacity)
        full_design = last_full_cap;

    if (!watt_as_unit) {
        present_rate = (((float)voltage / 1000.0) * ((float)present_rate / 1000.0));
        remaining = (((float)voltage / 1000.0) * ((float)remaining / 1000.0));
        full_design = (((float)voltage / 1000.0) * ((float)full_design / 1000.0));
    }

    float percentage_remaining =
        (((float)remaining / (float)full_design) * 100);

    if (integer_battery_capacity)
        (void)snprintf(percentagebuf,
                       sizeof(percentagebuf),
                       "%d%s",
                       (int)percentage_remaining, pct_mark);
    else
        (void)snprintf(percentagebuf,
                       sizeof(percentagebuf),
                       "%.02f%s",
                       percentage_remaining, pct_mark);

    /*
     * Handle percentage low_threshold here, and time low_threshold when
     * we have it.
     */
    if (status == CS_DISCHARGING && low_threshold > 0) {
        if (strcasecmp(threshold_type, "percentage") == 0 && (((float)remaining / (float)full_design) * 100) < low_threshold) {
            START_COLOR("color_bad");
            colorful_output = true;
        }
    }

    if (is_full)
        (void)snprintf(statusbuf, sizeof(statusbuf), "%s", BATT_STATUS_NAME(CS_FULL));
    else
        (void)snprintf(statusbuf, sizeof(statusbuf), "%s", BATT_STATUS_NAME(status));

    /*
     * The envsys(4) ACPI routines do not appear to provide a 'time
     * remaining' figure, so we must deduce it.
     */
    float remaining_time;
    int seconds, hours, minutes, seconds_remaining;

    if (status == CS_CHARGING)
        remaining_time = ((float)full_design - (float)remaining) / (float)present_rate;
    else if (status == CS_DISCHARGING)
        remaining_time = ((float)remaining / (float)present_rate);
    else
        remaining_time = 0;

    seconds_remaining = (int)(remaining_time * 3600.0);

    hours = seconds_remaining / 3600;
    seconds = seconds_remaining - (hours * 3600);
    minutes = seconds / 60;
    seconds -= (minutes * 60);

    if (status != CS_CHARGING) {
        if (hide_seconds)
            (void)snprintf(remainingbuf, sizeof(remainingbuf), "%02d:%02d",
                           max(hours, 0), max(minutes, 0));
        else
            (void)snprintf(remainingbuf, sizeof(remainingbuf), "%02d:%02d:%02d",
                           max(hours, 0), max(minutes, 0), max(seconds, 0));

        if (low_threshold > 0) {
            if (strcasecmp(threshold_type, "time") == 0 && ((float)seconds_remaining / 60.0) < (u_int)low_threshold) {
                START_COLOR("color_bad");
                colorful_output = true;
            }
        }
    } else {
        if (hide_seconds)
            (void)snprintf(remainingbuf, sizeof(remainingbuf), "(%02d:%02d until full)",
                           max(hours, 0), max(minutes, 0));
        else
            (void)snprintf(remainingbuf, sizeof(remainingbuf), "(%02d:%02d:%02d until full)",
                           max(hours, 0), max(minutes, 0), max(seconds, 0));
    }

    empty_time = time(NULL);
    empty_time += seconds_remaining;
    empty_tm = localtime(&empty_time);

    /* No need to show empty time if battery is charging */
    if (status != CS_CHARGING) {
        if (hide_seconds)
            (void)snprintf(emptytimebuf, sizeof(emptytimebuf), "%02d:%02d",
                           max(empty_tm->tm_hour, 0), max(empty_tm->tm_min, 0));
        else
            (void)snprintf(emptytimebuf, sizeof(emptytimebuf), "%02d:%02d:%02d",
                           max(empty_tm->tm_hour, 0), max(empty_tm->tm_min, 0), max(empty_tm->tm_sec, 0));
    }

    (void)snprintf(consumptionbuf, sizeof(consumptionbuf), "%1.2fW",
                   ((float)present_rate / 1000.0 / 1000.0));
#endif

#define EAT_SPACE_FROM_OUTPUT_IF_EMPTY(_buf)              \
    do {                                                  \
        if (strlen(_buf) == 0) {                          \
            if (outwalk > buffer && isspace(outwalk[-1])) \
                outwalk--;                                \
            else if (isspace(*(walk + 1)))                \
                walk++;                                   \
        }                                                 \
    } while (0)

    for (walk = format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "status")) {
            outwalk += sprintf(outwalk, "%s", statusbuf);
            walk += strlen("status");
        } else if (BEGINS_WITH(walk + 1, "percentage")) {
            outwalk += sprintf(outwalk, "%s", percentagebuf);
            walk += strlen("percentage");
        } else if (BEGINS_WITH(walk + 1, "remaining")) {
            outwalk += sprintf(outwalk, "%s", remainingbuf);
            walk += strlen("remaining");
            EAT_SPACE_FROM_OUTPUT_IF_EMPTY(remainingbuf);
        } else if (BEGINS_WITH(walk + 1, "emptytime")) {
            outwalk += sprintf(outwalk, "%s", emptytimebuf);
            walk += strlen("emptytime");
            EAT_SPACE_FROM_OUTPUT_IF_EMPTY(emptytimebuf);
        } else if (BEGINS_WITH(walk + 1, "consumption")) {
            outwalk += sprintf(outwalk, "%s", consumptionbuf);
            walk += strlen("consumption");
            EAT_SPACE_FROM_OUTPUT_IF_EMPTY(consumptionbuf);
        }
    }

    if (colorful_output)
        END_COLOR;

    OUTPUT_FULL_TEXT(buffer);
}
