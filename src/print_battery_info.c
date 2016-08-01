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

struct battery_info {
    int present_rate;
    int seconds_remaining;
    float percentage_remaining;
    charging_status_t status;
};

/*
 * Estimate the number of seconds remaining in state 'status'.
 *
 * Assumes a constant (dis)charge rate.
 */
#if defined(LINUX) || defined(__NetBSD__)
static int seconds_remaining_from_rate(charging_status_t status, float full_design, float remaining, float present_rate) {
    if (status == CS_CHARGING)
        return 3600.0 * (full_design - remaining) / present_rate;
    else if (status == CS_DISCHARGING)
        return 3600.0 * remaining / present_rate;
    else
        return 0;
}
#endif

static bool slurp_battery_info(struct battery_info *batt_info, yajl_gen json_gen, char *buffer, int number, const char *path, const char *format_down, bool last_full_capacity) {
    char *outwalk = buffer;

#if defined(LINUX)
    char buf[1024];
    const char *walk, *last;
    bool watt_as_unit = false;
    int full_design = -1,
        remaining = -1,
        voltage = -1;
    char batpath[512];
    sprintf(batpath, path, number);
    INSTANCE(batpath);

    if (!slurp(batpath, buf, sizeof(buf))) {
        OUTPUT_FULL_TEXT(format_down);
        return false;
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
            batt_info->present_rate = abs(atoi(walk + 1));
        else if (BEGINS_WITH(last, "POWER_SUPPLY_VOLTAGE_NOW"))
            voltage = abs(atoi(walk + 1));
        /* on some systems POWER_SUPPLY_POWER_NOW does not exist, but actually
         * it is the same as POWER_SUPPLY_CURRENT_NOW but with μWh as
         * unit instead of μAh. We will calculate it as we need it
         * later. */
        else if (BEGINS_WITH(last, "POWER_SUPPLY_POWER_NOW"))
            batt_info->present_rate = abs(atoi(walk + 1));
        else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Charging"))
            batt_info->status = CS_CHARGING;
        else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Full"))
            batt_info->status = CS_FULL;
        else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Discharging"))
            batt_info->status = CS_DISCHARGING;
        else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS="))
            batt_info->status = CS_UNKNOWN;
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
        batt_info->present_rate = (((float)voltage / 1000.0) * ((float)batt_info->present_rate / 1000.0));

        if (voltage != -1) {
            remaining = (((float)voltage / 1000.0) * ((float)remaining / 1000.0));
            full_design = (((float)voltage / 1000.0) * ((float)full_design / 1000.0));
        }
    }

    if ((full_design == -1) || (remaining == -1)) {
        OUTPUT_FULL_TEXT(format_down);
        return false;
    }

    batt_info->percentage_remaining = (((float)remaining / (float)full_design) * 100);
    /* Some batteries report POWER_SUPPLY_CHARGE_NOW=<full_design> when fully
     * charged, even though that’s plainly wrong. For people who chose to see
     * the percentage calculated based on the last full capacity, we clamp the
     * value to 100%, as that makes more sense.
     * See http://bugs.debian.org/785398 */
    if (last_full_capacity && batt_info->percentage_remaining > 100) {
        batt_info->percentage_remaining = 100;
    }

    if (batt_info->present_rate > 0 && batt_info->status != CS_FULL) {
        batt_info->seconds_remaining = seconds_remaining_from_rate(batt_info->status, full_design, remaining, batt_info->present_rate);
    }
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
    int state;
    int sysctl_rslt;
    size_t sysctl_size = sizeof(sysctl_rslt);

    if (sysctlbyname(BATT_LIFE, &sysctl_rslt, &sysctl_size, NULL, 0) != 0) {
        OUTPUT_FULL_TEXT(format_down);
        return false;
    }

    batt_info->percentage_remaining = sysctl_rslt;
    if (sysctlbyname(BATT_TIME, &sysctl_rslt, &sysctl_size, NULL, 0) != 0) {
        OUTPUT_FULL_TEXT(format_down);
        return false;
    }

    batt_info->seconds_remaining = sysctl_rslt * 60;
    if (sysctlbyname(BATT_STATE, &sysctl_rslt, &sysctl_size, NULL, 0) != 0) {
        OUTPUT_FULL_TEXT(format_down);
        return false;
    }

    state = sysctl_rslt;
    if (state == 0 && batt_info->percentage_remaining == 100)
        batt_info->status = CS_FULL;
    else if ((state & ACPI_BATT_STAT_CHARGING) && batt_info->percentage_remaining < 100)
        batt_info->status = CS_CHARGING;
    else
        batt_info->status = CS_DISCHARGING;
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
        return false;
    }
    if (ioctl(apm_fd, APM_IOC_GETPOWER, &apm_info) < 0)
        OUTPUT_FULL_TEXT("can't read power info");

    close(apm_fd);

    /* Don't bother to go further if there's no battery present. */
    if ((apm_info.battery_state == APM_BATTERY_ABSENT) ||
        (apm_info.battery_state == APM_BATT_UNKNOWN)) {
        OUTPUT_FULL_TEXT(format_down);
        return false;
    }

    switch (apm_info.ac_state) {
        case APM_AC_OFF:
            batt_info->status = CS_DISCHARGING;
            break;
        case APM_AC_ON:
            batt_info->status = CS_CHARGING;
            break;
        default:
            /* If we don't know what's going on, just assume we're discharging. */
            batt_info->status = CS_DISCHARGING;
            break;
    }

    batt_info->percentage_remaining = apm_info.battery_life;

    /* Can't give a meaningful value for remaining minutes if we're charging. */
    if (batt_info->status != CS_CHARGING) {
        batt_info->seconds_remaining = apm_info.minutes_left * 60;
    }
#elif defined(__NetBSD__)
    /*
     * Using envsys(4) via sysmon(4).
     */
    bool watt_as_unit = false;
    int full_design = -1,
        remaining = -1,
        voltage = -1;
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
        return false;
    }

    rval = prop_dictionary_recv_ioctl(fd, ENVSYS_GETDICTIONARY, &dict);
    if (rval == -1) {
        close(fd);
        return false;
    }

    if (prop_dictionary_count(dict) == 0) {
        prop_object_release(dict);
        close(fd);
        return false;
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
            return false;
        }

        iter2 = prop_array_iterator(array);
        if (!iter2) {
            prop_object_iterator_release(iter);
            prop_object_release(dict);
            close(fd);
            return false;
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
                    batt_info->status = CS_CHARGING;
                else
                    batt_info->status = CS_DISCHARGING;

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
                batt_info->present_rate = prop_number_integer_value(obj3);
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
        return false;
    }

    if (last_full_capacity)
        full_design = last_full_cap;

    if (!watt_as_unit) {
        batt_info->present_rate = (((float)voltage / 1000.0) * ((float)batt_info->present_rate / 1000.0));
        remaining = (((float)voltage / 1000.0) * ((float)remaining / 1000.0));
        full_design = (((float)voltage / 1000.0) * ((float)full_design / 1000.0));
    }

    batt_info->percentage_remaining =
        (((float)remaining / (float)full_design) * 100);

    if (is_full)
        batt_info->status = CS_FULL;

    /*
     * The envsys(4) ACPI routines do not appear to provide a 'time
     * remaining' figure, so we must deduce it.
     */
    batt_info->seconds_remaining = seconds_remaining_from_rate(batt_info->status, full_design, remaining, batt_info->present_rate);
#endif

    return true;
}

void print_battery_info(yajl_gen json_gen, char *buffer, int number, const char *path, const char *format, const char *format_down, const char *status_chr, const char *status_bat, const char *status_unk, const char *status_full, int low_threshold, char *threshold_type, bool last_full_capacity, bool integer_battery_capacity, bool hide_seconds) {
    const char *walk;
    char *outwalk = buffer;
    struct battery_info batt_info = {
        .present_rate = -1,
        .seconds_remaining = -1,
        .percentage_remaining = -1,
        .status = CS_DISCHARGING,
    };
    bool colorful_output = false;

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__) || defined(__OpenBSD__)
    /* These OSes report battery stats in whole percent. */
    integer_battery_capacity = true;
#endif
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
    /* These OSes report battery time in minutes. */
    hide_seconds = true;
#endif

    if (!slurp_battery_info(&batt_info, json_gen, buffer, number, path, format_down, last_full_capacity))
        return;

    if (batt_info.status == CS_DISCHARGING && low_threshold > 0) {
        if (batt_info.percentage_remaining >= 0 && strcasecmp(threshold_type, "percentage") == 0 && batt_info.percentage_remaining < low_threshold) {
            START_COLOR("color_bad");
            colorful_output = true;
        } else if (batt_info.seconds_remaining >= 0 && strcasecmp(threshold_type, "time") == 0 && batt_info.seconds_remaining < 60 * low_threshold) {
            START_COLOR("color_bad");
            colorful_output = true;
        }
    }

#define EAT_SPACE_FROM_OUTPUT_IF_NO_OUTPUT()              \
    do {                                                  \
        if (outwalk == prevoutwalk) {                     \
            if (outwalk > buffer && isspace(outwalk[-1])) \
                outwalk--;                                \
            else if (isspace(*(walk + 1)))                \
                walk++;                                   \
        }                                                 \
    } while (0)

    for (walk = format; *walk != '\0'; walk++) {
        char *prevoutwalk = outwalk;

        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "status")) {
            const char *statusstr;
            switch (batt_info.status) {
                case CS_CHARGING:
                    statusstr = status_chr;
                    break;
                case CS_DISCHARGING:
                    statusstr = status_bat;
                    break;
                case CS_FULL:
                    statusstr = status_full;
                    break;
                default:
                    statusstr = status_unk;
            }

            outwalk += sprintf(outwalk, "%s", statusstr);
            walk += strlen("status");
        } else if (BEGINS_WITH(walk + 1, "percentage")) {
            if (integer_battery_capacity) {
                outwalk += sprintf(outwalk, "%.00f%s", batt_info.percentage_remaining, pct_mark);
            } else {
                outwalk += sprintf(outwalk, "%.02f%s", batt_info.percentage_remaining, pct_mark);
            }
            walk += strlen("percentage");
        } else if (BEGINS_WITH(walk + 1, "remaining")) {
            if (batt_info.seconds_remaining >= 0) {
                int seconds, hours, minutes;

                hours = batt_info.seconds_remaining / 3600;
                seconds = batt_info.seconds_remaining - (hours * 3600);
                minutes = seconds / 60;
                seconds -= (minutes * 60);

                if (hide_seconds)
                    outwalk += sprintf(outwalk, "%02d:%02d",
                                       max(hours, 0), max(minutes, 0));
                else
                    outwalk += sprintf(outwalk, "%02d:%02d:%02d",
                                       max(hours, 0), max(minutes, 0), max(seconds, 0));
            }
            walk += strlen("remaining");
            EAT_SPACE_FROM_OUTPUT_IF_NO_OUTPUT();
        } else if (BEGINS_WITH(walk + 1, "emptytime")) {
            if (batt_info.seconds_remaining >= 0) {
                time_t empty_time = time(NULL) + batt_info.seconds_remaining;
                struct tm *empty_tm = localtime(&empty_time);

                if (hide_seconds)
                    outwalk += sprintf(outwalk, "%02d:%02d",
                                       max(empty_tm->tm_hour, 0), max(empty_tm->tm_min, 0));
                else
                    outwalk += sprintf(outwalk, "%02d:%02d:%02d",
                                       max(empty_tm->tm_hour, 0), max(empty_tm->tm_min, 0), max(empty_tm->tm_sec, 0));
            }
            walk += strlen("emptytime");
            EAT_SPACE_FROM_OUTPUT_IF_NO_OUTPUT();
        } else if (BEGINS_WITH(walk + 1, "consumption")) {
            if (batt_info.present_rate >= 0)
                outwalk += sprintf(outwalk, "%1.2fW", batt_info.present_rate / 1e6);

            walk += strlen("consumption");
            EAT_SPACE_FROM_OUTPUT_IF_NO_OUTPUT();
        }
    }

    if (colorful_output)
        END_COLOR;

    OUTPUT_FULL_TEXT(buffer);
}
