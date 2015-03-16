// vim:ts=4:sw=4:expandtab
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

/* define fixed output-Strings */
char *season_long[5] = {
    "Chaos",
    "Discord",
    "Confusion",
    "Bureaucracy",
    "The Aftermath"};

char *season_short[5] = {
    "Chs",
    "Dsc",
    "Cfn",
    "Bcy",
    "Afm"};

char *day_long[5] = {
    "Sweetmorn",
    "Boomtime",
    "Pungenday",
    "Prickle-Prickle",
    "Setting Orange"};

char *day_short[5] = {
    "SM",
    "BT",
    "PD",
    "PP",
    "SO"};

char *holidays[10] = {
    "Mungday",
    "Mojoday",
    "Syaday",
    "Zaraday",
    "Maladay",
    "Chaoflux",
    "Discoflux",
    "Confuflux",
    "Bureflux",
    "Afflux"};

/* A helper-struct, taking the discordian date */
struct disc_time {
    int year;
    int season;
    int week_day;
    int season_day;
    int st_tibs_day;
};

/* Print the date *dt in format *format */
static int format_output(char *outwalk, char *format, struct disc_time *dt) {
    char *orig_outwalk = outwalk;
    char *i;
    char *tibs_end = 0;

    for (i = format; *i != '\0'; i++) {
        if (*i != '%') {
            *(outwalk++) = *i;
            continue;
        }
        switch (*(i + 1)) {
            /* Weekday in long and abbreviation */
            case 'A':
                outwalk += sprintf(outwalk, "%s", day_long[dt->week_day]);
                break;
            case 'a':
                outwalk += sprintf(outwalk, "%s", day_short[dt->week_day]);
                break;
            /* Season in long and abbreviation */
            case 'B':
                outwalk += sprintf(outwalk, "%s", season_long[dt->season]);
                break;
            case 'b':
                outwalk += sprintf(outwalk, "%s", season_short[dt->season]);
                break;
            /* Day of the season (ordinal and cardinal) */
            case 'd':
                outwalk += sprintf(outwalk, "%d", dt->season_day + 1);
                break;
            case 'e':
                outwalk += sprintf(outwalk, "%d", dt->season_day + 1);
                if (dt->season_day > 9 && dt->season_day < 13) {
                    outwalk += sprintf(outwalk, "th");
                    break;
                }

                switch (dt->season_day % 10) {
                    case 0:
                        outwalk += sprintf(outwalk, "st");
                        break;
                    case 1:
                        outwalk += sprintf(outwalk, "nd");
                        break;
                    case 2:
                        outwalk += sprintf(outwalk, "rd");
                        break;
                    default:
                        outwalk += sprintf(outwalk, "th");
                        break;
                }
                break;
            /* YOLD */
            case 'Y':
                outwalk += sprintf(outwalk, "%d", dt->year);
                break;
            /* Holidays */
            case 'H':
                if (dt->season_day == 4) {
                    outwalk += sprintf(outwalk, "%s", holidays[dt->season]);
                }
                if (dt->season_day == 49) {
                    outwalk += sprintf(outwalk, "%s", holidays[dt->season + 5]);
                }
                break;
            /* Stop parsing the format string, except on Holidays */
            case 'N':
                if (dt->season_day != 4 && dt->season_day != 49) {
                    return (outwalk - orig_outwalk);
                }
                break;
            /* Newline- and Tabbing-characters */
            case 'n':
                outwalk += sprintf(outwalk, "\n");
                break;
            case 't':
                outwalk += sprintf(outwalk, "\t");
                break;
            /* The St. Tib's Day replacement */
            case '{':
                tibs_end = strstr(i, "%}");
                if (tibs_end == NULL) {
                    i++;
                    break;
                }
                if (dt->st_tibs_day) {
                    /* We outpt "St. Tib's Day... */
                    outwalk += sprintf(outwalk, "St. Tib's Day");
                } else {
                    /* ...or parse the substring between %{ and %} ... */
                    *tibs_end = '\0';
                    outwalk += format_output(outwalk, i + 2, dt);
                    *tibs_end = '%';
                }
                /* ...and continue with the rest */
                i = tibs_end;
                break;
            case '}':
                i++;
                break;
            default:
                /* No escape-sequence, so we just skip */
                outwalk += sprintf(outwalk, "%%%c", *(i + 1));
                break;
        }
        i++;
    }
    return (outwalk - orig_outwalk);
}

/* Get the current date and convert it to discordian */
struct disc_time *get_ddate(struct tm *current_tm) {
    static struct disc_time dt;

    if (current_tm == NULL)
        return NULL;

    /* We have to know, whether we have to insert St. Tib's Day, so whether it's a leap
     * year in gregorian calendar */
    int is_leap_year = !(current_tm->tm_year % 4) &&
                       (!(current_tm->tm_year % 400) || current_tm->tm_year % 100);

    /* If St. Tib's Day has passed, it will be necessary to skip a day. */
    int yday = current_tm->tm_yday;

    if (is_leap_year && yday == 59) {
        /* On St. Tibs Day we don't have to define a date */
        dt.st_tibs_day = 1;
    } else {
        dt.st_tibs_day = 0;
        if (is_leap_year && yday > 59)
            yday -= 1;

        dt.season_day = yday % 73;
        dt.week_day = yday % 5;
    }
    dt.year = current_tm->tm_year + 3066;
    dt.season = yday / 73;
    return &dt;
}

void print_ddate(yajl_gen json_gen, char *buffer, const char *format, time_t t) {
    char *outwalk = buffer;
    static char *form = NULL;
    struct tm current_tm;
    struct disc_time *dt;
    set_timezone(NULL); /* Use local time. */
    localtime_r(&t, &current_tm);
    if ((dt = get_ddate(&current_tm)) == NULL)
        return;
    if (form == NULL)
        if ((form = malloc(strlen(format) + 1)) == NULL)
            return;
    strcpy(form, format);
    outwalk += format_output(outwalk, form, dt);
    OUTPUT_FULL_TEXT(buffer);
}
