// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#define STRING_SIZE 20

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

void print_ddate(ddate_ctx_t *ctx) {
    char *outwalk = ctx->buf;
    struct tm current_tm;
    struct disc_time *dt;
    set_timezone(NULL); /* Use local time. */
    localtime_r(&ctx->t, &current_tm);
    if ((dt = get_ddate(&current_tm)) == NULL)
        return;

    char string_A[STRING_SIZE];
    char string_a[STRING_SIZE];
    char string_B[STRING_SIZE];
    char string_b[STRING_SIZE];
    char string_d[STRING_SIZE];
    char string_e[STRING_SIZE];
    char string_Y[STRING_SIZE];
    char string_H[STRING_SIZE];
    char string_N[STRING_SIZE];
    /* Newline- and Tabbing-characters */
    char string_n[2] = "\n";
    char string_t[2] = "\t";
    char string_tibs_day[14] = "St. Tib's Day";

    /* Weekday in long and abbreviation */
    snprintf(string_A, STRING_SIZE, "%s", day_long[dt->week_day]);
    snprintf(string_a, STRING_SIZE, "%s", day_short[dt->week_day]);
    /* Season in long and abbreviation */
    snprintf(string_B, STRING_SIZE, "%s", season_long[dt->season]);
    snprintf(string_b, STRING_SIZE, "%s", season_short[dt->season]);
    /* Day of the season (ordinal and cardinal) */
    snprintf(string_d, STRING_SIZE, "%d", dt->season_day + 1);
    snprintf(string_e, STRING_SIZE, "%d", dt->season_day + 1);
    if (dt->season_day > 9 && dt->season_day < 13) {
        strcat(string_e, "th");
    }
    switch (dt->season_day % 10) {
        case 0:
            strcat(string_e, "st");
            break;
        case 1:
            strcat(string_e, "nd");
            break;
        case 2:
            strcat(string_e, "rd");
            break;
        default:
            strcat(string_e, "th");
            break;
    }
    /* YOLD */
    snprintf(string_Y, STRING_SIZE, "%d", dt->year);
    /* Holidays */
    if (dt->season_day == 4) {
        snprintf(string_H, STRING_SIZE, "%s", holidays[dt->season]);
    }
    if (dt->season_day == 49) {
        snprintf(string_H, STRING_SIZE, "%s", holidays[dt->season + 5]);
    }
    /* Stop parsing the format string, except on Holidays */
    if (dt->season_day != 4 && dt->season_day != 49) {
        snprintf(string_N, STRING_SIZE, "%s", "\0");
    }

    placeholder_t placeholders[] = {
        {.name = "%A", .value = string_A},
        {.name = "%a", .value = string_a},
        {.name = "%B", .value = string_B},
        {.name = "%b", .value = string_b},
        {.name = "%d", .value = string_d},
        {.name = "%e", .value = string_e},
        {.name = "%Y", .value = string_Y},
        {.name = "%H", .value = string_H},
        {.name = "%N", .value = string_N},
        {.name = "%n", .value = string_n},
        {.name = "%t", .value = string_t},
        {.name = "%{", .value = string_tibs_day},
        {.name = "%}", .value = ""}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    char *formatted = format_placeholders(ctx->format, &placeholders[0], num);
    OUTPUT_FORMATTED;
    free(formatted);
    OUTPUT_FULL_TEXT(ctx->buf);
}
