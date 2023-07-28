// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#define STRING_SIZE 10

#if defined(__linux__)
#include <errno.h>
#include <glob.h>
#include <sys/types.h>
#endif

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#include <dev/acpica/acpiio.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

#if defined(__DragonFly__)
#include <sys/fcntl.h>
#endif

#if defined(__OpenBSD__)
#include <machine/apmvar.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>
#include <sys/sysctl.h>
#endif

#if defined(__NetBSD__)
#include <fcntl.h>
#include <prop/proplib.h>
#include <sys/envsys.h>
#endif

typedef enum {
    CS_UNKNOWN,
    CS_DISCHARGING,
    CS_CHARGING,
    CS_FULL,
    CS_IDLE,
} charging_status_t;

/* A description of the state of one or more batteries. */
struct battery_info {
    /* measured properties */
    int full_design;  /* in uAh */
    int full_last;    /* in uAh */
    int remaining;    /* in uAh */
    int present_rate; /* in uA, always non-negative */

    /* derived properties */
    int seconds_remaining;
    float percentage_remaining;
    charging_status_t status;
};

#if defined(__DragonFly__)
#define ACPIDEV "/dev/acpi"
static int acpifd;

static bool acpi_init(void) {
    if (acpifd == 0) {
        acpifd = open(ACPIDEV, O_RDWR);
        if (acpifd == -1)
            acpifd = open(ACPIDEV, O_RDONLY);
        if (acpifd == -1)
            return false;
    }
    return true;
}
#endif

#if defined(__linux__) || defined(__NetBSD__)
/*
 * Add batt_info data to acc.
 */
static void add_battery_info(struct battery_info *acc, const struct battery_info *batt_info) {
    if (acc->remaining < 0) {
        /* initialize accumulator so we can add to it */
        acc->full_design = 0;
        acc->full_last = 0;
        acc->remaining = 0;
        acc->present_rate = 0;
    }

    acc->full_design += batt_info->full_design;
    acc->full_last += batt_info->full_last;
    acc->remaining += batt_info->remaining;

    /* make present_rate negative for discharging and positive for charging */
    int present_rate = (acc->status == CS_DISCHARGING ? -1 : 1) * acc->present_rate;
    present_rate += (batt_info->status == CS_DISCHARGING ? -1 : 1) * batt_info->present_rate;

    /* merge status */
    switch (acc->status) {
        case CS_UNKNOWN:
            acc->status = batt_info->status;
            break;

        case CS_DISCHARGING:
            if (present_rate > 0)
                acc->status = CS_CHARGING;
            /* else if batt_info is DISCHARGING: no conflict
             * else if batt_info is CHARGING: present_rate should indicate that
             * else if batt_info is FULL: but something else is discharging */
            break;

        case CS_CHARGING:
            if (present_rate < 0)
                acc->status = CS_DISCHARGING;
            /* else if batt_info is DISCHARGING: present_rate should indicate that
             * else if batt_info is CHARGING: no conflict
             * else if batt_info is FULL: but something else is charging */
            break;

        case CS_FULL:
            if (batt_info->status != CS_UNKNOWN)
                acc->status = batt_info->status;
            /* else: retain FULL, since it is more specific than UNKNOWN */
            break;

        case CS_IDLE:
            if (batt_info->status != CS_UNKNOWN && batt_info->status != CS_FULL)
                acc->status = batt_info->status;
            /* else: retain IDLE, since it is more specific than UNKNOWN and is
             * implied by CS_FULL though correct for all batteries */
            break;
    }

    acc->present_rate = abs(present_rate);
}
#endif

static bool slurp_battery_info(battery_info_ctx_t *ctx, struct battery_info *batt_info, yajl_gen json_gen, char *buffer, int number, const char *path, const char *format_down) {
    char *outwalk = buffer;

#if defined(__linux__)
    char buf[1024];
    const char *walk, *last;
    bool watt_as_unit = false;
    int voltage = -1;
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

        if (BEGINS_WITH(last, "POWER_SUPPLY_ENERGY_NOW=")) {
            watt_as_unit = true;
            batt_info->remaining = atoi(walk + 1);
            batt_info->percentage_remaining = -1;
        } else if (BEGINS_WITH(last, "POWER_SUPPLY_CHARGE_NOW=")) {
            watt_as_unit = false;
            batt_info->remaining = atoi(walk + 1);
            batt_info->percentage_remaining = -1;
        } else if (BEGINS_WITH(last, "POWER_SUPPLY_CAPACITY=") && batt_info->remaining == -1) {
            batt_info->percentage_remaining = atoi(walk + 1);
        } else if (BEGINS_WITH(last, "POWER_SUPPLY_CURRENT_NOW="))
            batt_info->present_rate = abs(atoi(walk + 1));
        else if (BEGINS_WITH(last, "POWER_SUPPLY_VOLTAGE_NOW="))
            voltage = abs(atoi(walk + 1));
        else if (BEGINS_WITH(last, "POWER_SUPPLY_TIME_TO_EMPTY_NOW="))
            batt_info->seconds_remaining = abs(atoi(walk + 1)) * 60;
        /* on some systems POWER_SUPPLY_POWER_NOW does not exist, but actually
         * it is the same as POWER_SUPPLY_CURRENT_NOW but with μWh as
         * unit instead of μAh. We will calculate it as we need it
         * later. */
        else if (BEGINS_WITH(last, "POWER_SUPPLY_POWER_NOW="))
            batt_info->present_rate = abs(atoi(walk + 1));
        else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Charging"))
            batt_info->status = CS_CHARGING;
        else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Full"))
            batt_info->status = CS_FULL;
        else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Discharging"))
            batt_info->status = CS_DISCHARGING;
        else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Not charging"))
            batt_info->status = CS_IDLE;
        else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS="))
            batt_info->status = CS_UNKNOWN;
        else if (BEGINS_WITH(last, "POWER_SUPPLY_CHARGE_FULL_DESIGN=") ||
                 BEGINS_WITH(last, "POWER_SUPPLY_ENERGY_FULL_DESIGN="))
            batt_info->full_design = atoi(walk + 1);
        else if (BEGINS_WITH(last, "POWER_SUPPLY_ENERGY_FULL=") ||
                 BEGINS_WITH(last, "POWER_SUPPLY_CHARGE_FULL="))
            batt_info->full_last = atoi(walk + 1);
    }

    /* the difference between POWER_SUPPLY_ENERGY_NOW and
     * POWER_SUPPLY_CHARGE_NOW is the unit of measurement. The energy is
     * given in mWh, the charge in mAh. So calculate every value given in
     * ampere to watt */
    if (!watt_as_unit && voltage >= 0) {
        if (batt_info->present_rate > 0) {
            batt_info->present_rate = (((float)voltage / 1000.0) * ((float)batt_info->present_rate / 1000.0));
        }
        if (batt_info->remaining > 0) {
            batt_info->remaining = (((float)voltage / 1000.0) * ((float)batt_info->remaining / 1000.0));
        }
        if (batt_info->full_design > 0) {
            batt_info->full_design = (((float)voltage / 1000.0) * ((float)batt_info->full_design / 1000.0));
        }
        if (batt_info->full_last > 0) {
            batt_info->full_last = (((float)voltage / 1000.0) * ((float)batt_info->full_last / 1000.0));
        }
    }
#elif defined(__DragonFly__)
    union acpi_battery_ioctl_arg battio;
    if (acpi_init()) {
        battio.unit = number;
        ioctl(acpifd, ACPIIO_BATT_GET_BIF, &battio);
        batt_info->full_design = battio.bif.dcap;
        batt_info->full_last = battio.bif.lfcap;
        battio.unit = number;
        ioctl(acpifd, ACPIIO_BATT_GET_BATTINFO, &battio);
        batt_info->percentage_remaining = battio.battinfo.cap;
        batt_info->present_rate = battio.battinfo.rate;
        batt_info->seconds_remaining = battio.battinfo.min * 60;
        switch (battio.battinfo.state) {
            case 0:
                batt_info->status = CS_FULL;
                break;
            case ACPI_BATT_STAT_CHARGING:
                batt_info->status = CS_CHARGING;
                break;
            case ACPI_BATT_STAT_DISCHARG:
                batt_info->status = CS_DISCHARGING;
                break;
            default:
                batt_info->status = CS_UNKNOWN;
        }
        OUTPUT_FULL_TEXT(format_down);
    }
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
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
	 * the generic interface on macppc/sparc64/zaurus.  Machines that have ACPI
	 * battery sensors gain some extra information.
	 */
    struct apm_power_info apm_info;
    struct sensordev sensordev;
    struct sensor sensor;
    size_t sdlen, slen;
    int apm_fd;
    int dev, mib[5] = {CTL_HW, HW_SENSORS, 0, 0, 0};
    int volts = 0;

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

    /* If acpibat* are present, check sensors for data not present via APM. */
    batt_info->present_rate = 0;
    sdlen = sizeof(sensordev);
    slen = sizeof(sensor);

    for (dev = 0;; dev++) {
        mib[2] = dev;
        if (sysctl(mib, 3, &sensordev, &sdlen, NULL, 0) == -1) {
            break;
        }
        /* 'path' is the node within the full path */
        if (BEGINS_WITH(sensordev.xname, "acpibat")) {
            /* power0 */
            mib[3] = SENSOR_WATTS;
            mib[4] = 0;
            if (sysctl(mib, 5, &sensor, &slen, NULL, 0) == -1) {
                /* try current0 */
                mib[3] = SENSOR_AMPS;
                if (sysctl(mib, 5, &sensor, &slen, NULL, 0) == -1)
                    continue;
                volts = sensor.value;

                /* we also need current voltage to convert amps to watts */
                mib[3] = SENSOR_VOLTS_DC;
                mib[4] = 1;
                if (sysctl(mib, 5, &sensor, &slen, NULL, 0) == -1)
                    continue;

                batt_info->present_rate += (((float)volts / 1000.0) * ((float)sensor.value / 1000.0));
            } else {
                batt_info->present_rate += sensor.value;
            }
        }
    }
#elif defined(__NetBSD__)
    /*
     * Using envsys(4) via sysmon(4).
     */
    int fd, rval;
    bool is_found = false;
    char sensor_desc[16];

    prop_dictionary_t dict;
    prop_array_t array;
    prop_object_iterator_t iter;
    prop_object_iterator_t iter2;
    prop_object_t obj, obj2, obj3, obj4, obj5;

    if (number >= 0)
        (void)snprintf(sensor_desc, sizeof(sensor_desc), "acpibat%d", number);

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
        if (number < 0) {
            /* we want all batteries */
            if (!BEGINS_WITH(prop_dictionary_keysym_cstring_nocopy(obj),
                             "acpibat"))
                continue;
        } else {
            /* we want a specific battery */
            if (strcmp(sensor_desc,
                       prop_dictionary_keysym_cstring_nocopy(obj)) != 0)
                continue;
        }

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

        struct battery_info batt_buf = {
            .full_design = 0,
            .full_last = 0,
            .remaining = 0,
            .present_rate = 0,
            .status = CS_UNKNOWN,
        };
        int voltage = -1;
        bool watt_as_unit = false;

        /* iterate over array of dicts specific to target battery */
        while ((obj2 = prop_object_iterator_next(iter2)) != NULL) {
            obj3 = prop_dictionary_get(obj2, "description");

            if (obj3 == NULL)
                continue;

            if (strcmp("charging", prop_string_cstring_nocopy(obj3)) == 0) {
                obj3 = prop_dictionary_get(obj2, "cur-value");

                if (prop_number_integer_value(obj3))
                    batt_buf.status = CS_CHARGING;
                else
                    batt_buf.status = CS_DISCHARGING;
            } else if (strcmp("charge", prop_string_cstring_nocopy(obj3)) == 0) {
                obj3 = prop_dictionary_get(obj2, "cur-value");
                obj4 = prop_dictionary_get(obj2, "max-value");
                obj5 = prop_dictionary_get(obj2, "type");

                batt_buf.remaining = prop_number_integer_value(obj3);
                batt_buf.full_design = prop_number_integer_value(obj4);

                if (strcmp("Ampere hour", prop_string_cstring_nocopy(obj5)) == 0)
                    watt_as_unit = false;
                else
                    watt_as_unit = true;
            } else if (strcmp("discharge rate", prop_string_cstring_nocopy(obj3)) == 0) {
                obj3 = prop_dictionary_get(obj2, "cur-value");
                batt_buf.present_rate = prop_number_integer_value(obj3);
            } else if (strcmp("charge rate", prop_string_cstring_nocopy(obj3)) == 0) {
                obj3 = prop_dictionary_get(obj2, "cur-value");
                batt_info->present_rate = prop_number_integer_value(obj3);
            } else if (strcmp("last full cap", prop_string_cstring_nocopy(obj3)) == 0) {
                obj3 = prop_dictionary_get(obj2, "cur-value");
                batt_buf.full_last = prop_number_integer_value(obj3);
            } else if (strcmp("voltage", prop_string_cstring_nocopy(obj3)) == 0) {
                obj3 = prop_dictionary_get(obj2, "cur-value");
                voltage = prop_number_integer_value(obj3);
            }
        }
        prop_object_iterator_release(iter2);

        if (!watt_as_unit && voltage != -1) {
            batt_buf.present_rate = (((float)voltage / 1000.0) * ((float)batt_buf.present_rate / 1000.0));
            batt_buf.remaining = (((float)voltage / 1000.0) * ((float)batt_buf.remaining / 1000.0));
            batt_buf.full_design = (((float)voltage / 1000.0) * ((float)batt_buf.full_design / 1000.0));
            batt_buf.full_last = (((float)voltage / 1000.0) * ((float)batt_buf.full_last / 1000.0));
        }

        if (batt_buf.remaining == batt_buf.full_design)
            batt_buf.status = CS_FULL;

        add_battery_info(batt_info, &batt_buf);
    }

    prop_object_iterator_release(iter);
    prop_object_release(dict);
    close(fd);

    if (!is_found) {
        OUTPUT_FULL_TEXT(format_down);
        return false;
    }

    batt_info->present_rate = abs(batt_info->present_rate);
#endif

    return true;
}

/*
 * Populate batt_info with aggregate information about all batteries.
 * Returns false on error, and an error message will have been written.
 */
static bool slurp_all_batteries(battery_info_ctx_t *ctx, struct battery_info *batt_info, yajl_gen json_gen, char *buffer, const char *path, const char *format_down) {
#if defined(__linux__)
    char *outwalk = buffer;
    bool is_found = false;

    char *placeholder;
    char *globpath = sstrdup(path);
    if ((placeholder = strstr(path, "%d")) != NULL) {
        char *globplaceholder = globpath + (placeholder - path);
        *globplaceholder = '*';
        strcpy(globplaceholder + 1, placeholder + 2);
    }

    if (!strcmp(globpath, path)) {
        OUTPUT_FULL_TEXT("no '%d' in battery path");
        return false;
    }

    glob_t globbuf;
    if (glob(globpath, 0, NULL, &globbuf) == 0) {
        for (size_t i = 0; i < globbuf.gl_pathc; i++) {
            /* Probe to see if there is such a battery. */
            struct battery_info batt_buf = {
                .full_design = 0,
                .full_last = 0,
                .remaining = 0,
                .present_rate = 0,
                .status = CS_UNKNOWN,
            };
            if (!slurp_battery_info(ctx, &batt_buf, json_gen, buffer, i, globbuf.gl_pathv[i], format_down)) {
                globfree(&globbuf);
                free(globpath);
                return false;
            }

            is_found = true;
            add_battery_info(batt_info, &batt_buf);
        }
        globfree(&globbuf);
    }
    free(globpath);

    if (!is_found) {
        OUTPUT_FULL_TEXT(format_down);
        return false;
    }

    batt_info->present_rate = abs(batt_info->present_rate);
#else
    /* FreeBSD and OpenBSD only report aggregates. NetBSD always
     * iterates through all batteries, so it's more efficient to
     * aggregate in slurp_battery_info. */
    return slurp_battery_info(ctx, batt_info, json_gen, buffer, -1, path, format_down);
#endif

    return true;
}

void print_battery_info(battery_info_ctx_t *ctx) {
    char *outwalk = ctx->buf;
    struct battery_info batt_info = {
        .full_design = -1,
        .full_last = -1,
        .remaining = -1,
        .present_rate = -1,
        .seconds_remaining = -1,
        .percentage_remaining = -1,
        .status = CS_UNKNOWN,
    };
    bool colorful_output = false;

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__) || defined(__OpenBSD__)
    /* These OSes report battery stats in whole percent. */
    if (strcmp("%.02f%s", ctx->format_percentage) == 0) {
        ctx->format_percentage = "%.00f%s";
    }
#endif
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__) || defined(__OpenBSD__)
    /* These OSes report battery time in minutes. */
    ctx->hide_seconds = true;
#endif

    if (ctx->number < 0) {
        if (!slurp_all_batteries(ctx, &batt_info, ctx->json_gen, ctx->buf, ctx->path, ctx->format_down))
            return;
    } else {
        if (!slurp_battery_info(ctx, &batt_info, ctx->json_gen, ctx->buf, ctx->number, ctx->path, ctx->format_down))
            return;
    }

    // *Choose* a measure of the 'full' battery. It is whichever is better of
    // the battery's (hardware-given) design capacity (batt_info.full_design)
    // and the battery's last known good charge (batt_info.full_last).
    // We prefer the design capacity, but use the last capacity if we don't have it,
    // or if we are asked to (last_full_capacity == true); but similarly we use
    // the design capacity if we don't have the last capacity.
    // If we don't have either then both full_design and full_last <= 0,
    // which implies full <= 0, which bails out on the following line.
    int full = batt_info.full_design;
    if (full <= 0 || (ctx->last_full_capacity && batt_info.full_last > 0)) {
        full = batt_info.full_last;
    }
    if (full <= 0 && batt_info.remaining < 0 && batt_info.percentage_remaining < 0) {
        /* We have no physical measurements and no estimates. Nothing
         * much we can report, then. */
        OUTPUT_FULL_TEXT(ctx->format_down);
        return;
    }

    if (batt_info.percentage_remaining < 0) {
        batt_info.percentage_remaining = (((float)batt_info.remaining / (float)full) * 100);
        /* Some batteries report POWER_SUPPLY_CHARGE_NOW=<full_design> when fully
         * charged, even though that’s plainly wrong. For people who chose to see
         * the percentage calculated based on the last full capacity, we clamp the
         * value to 100%, as that makes more sense.
         * See http://bugs.debian.org/785398 */
        if (ctx->last_full_capacity && batt_info.percentage_remaining > 100) {
            batt_info.percentage_remaining = 100;
        }
    }

    if (batt_info.seconds_remaining < 0 && batt_info.present_rate > 0 && batt_info.status != CS_FULL) {
        if (batt_info.status == CS_CHARGING)
            batt_info.seconds_remaining = 3600.0 * (full - batt_info.remaining) / batt_info.present_rate;
        else if (batt_info.status == CS_DISCHARGING)
            batt_info.seconds_remaining = 3600.0 * batt_info.remaining / batt_info.present_rate;
        else
            batt_info.seconds_remaining = 0;
    }

    if (batt_info.status == CS_DISCHARGING && ctx->low_threshold > 0) {
        if (batt_info.percentage_remaining >= 0 && strcasecmp(ctx->threshold_type, "percentage") == 0 && batt_info.percentage_remaining < ctx->low_threshold) {
            START_COLOR("color_bad");
            colorful_output = true;
        } else if (batt_info.seconds_remaining >= 0 && strcasecmp(ctx->threshold_type, "time") == 0 && batt_info.seconds_remaining < 60 * ctx->low_threshold) {
            START_COLOR("color_bad");
            colorful_output = true;
        }
    }

    char string_status[STRING_SIZE];
    char string_percentage[STRING_SIZE];
    // following variables are not alwasy set. If they are not set they should be empty.
    char string_remaining[STRING_SIZE] = "";
    char string_emptytime[STRING_SIZE] = "";
    char string_consumption[STRING_SIZE] = "";

    const char *statusstr;
    switch (batt_info.status) {
        case CS_CHARGING:
            statusstr = ctx->status_chr;
            break;
        case CS_DISCHARGING:
            statusstr = ctx->status_bat;
            break;
        case CS_FULL:
            statusstr = ctx->status_full;
            break;
        case CS_IDLE:
            statusstr = ctx->status_idle;
            break;
        default:
            statusstr = ctx->status_unk;
    }
    snprintf(string_status, STRING_SIZE, "%s", statusstr);
    snprintf(string_percentage, STRING_SIZE, ctx->format_percentage, batt_info.percentage_remaining, pct_mark);

    if (batt_info.seconds_remaining >= 0) {
        int seconds, hours, minutes;
        hours = batt_info.seconds_remaining / 3600;
        seconds = batt_info.seconds_remaining - (hours * 3600);
        minutes = seconds / 60;
        seconds -= (minutes * 60);
        if (ctx->hide_seconds)
            snprintf(string_remaining, STRING_SIZE, "%02d:%02d", max(hours, 0), max(minutes, 0));
        else
            snprintf(string_remaining, STRING_SIZE, "%02d:%02d:%02d", max(hours, 0), max(minutes, 0), max(seconds, 0));
    }

    if (batt_info.seconds_remaining >= 0) {
        time_t empty_time = time(NULL) + batt_info.seconds_remaining;
        set_timezone(NULL); /* Use local time. */
        struct tm *empty_tm = localtime(&empty_time);
        if (ctx->hide_seconds)
            snprintf(string_emptytime, STRING_SIZE, "%02d:%02d", max(empty_tm->tm_hour, 0), max(empty_tm->tm_min, 0));
        else
            snprintf(string_emptytime, STRING_SIZE, "%02d:%02d:%02d", max(empty_tm->tm_hour, 0), max(empty_tm->tm_min, 0), max(empty_tm->tm_sec, 0));
    }

    if (batt_info.present_rate >= 0)
        snprintf(string_consumption, STRING_SIZE, "%1.2fW", batt_info.present_rate / 1e6);

    placeholder_t placeholders[] = {
        {.name = "%status", .value = string_status},
        {.name = "%percentage", .value = string_percentage},
        {.name = "%remaining", .value = string_remaining},
        {.name = "%emptytime", .value = string_emptytime},
        {.name = "%consumption", .value = string_consumption}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    char *untrimmed = format_placeholders(ctx->format, &placeholders[0], num);
    char *formatted = trim(untrimmed);
    OUTPUT_FORMATTED;
    free(formatted);
    free(untrimmed);

    if (colorful_output) {
        END_COLOR;
    }

    OUTPUT_FULL_TEXT(ctx->buf);
}
