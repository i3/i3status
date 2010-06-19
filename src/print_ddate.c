// vim:ts=8:expandtab
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* define fixed output-Strings */
char *season_long[5] = {
        "Chaos",
        "Discord",
        "Confusion",
        "Bureaucracy",
        "The Aftermath"
};

char *season_short[5] = {
        "Chs",
        "Dsc",
        "Cfn",
        "Bcy",
        "Afm"
};

char *day_long[5] = {
        "Sweetmorn",
        "Boomtime",
        "Pungenday",
        "Prickle-Prickle",
        "Setting Orange"
};

char *day_short[5] = {
        "SM",
        "BT",
        "PD",
        "PP",
        "SO"
};

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
        "Afflux"
};

/* A helper-struct, taking the discordian date */
struct disc_time {
        int year;
        int season;
        int week_day;
        int season_day;
        int st_tibs_day;
};

/* Print the date *dt in format *format */
int format_output(char *format, struct disc_time *dt) {
        char *i;
        char *tibs_end = 0;

        for (i = format; *i != '\0'; i++) {
                if (*i != '%') {
                        putchar(*i);
                        continue;
                }
                switch (*(i+1)) {
                        /* Weekday in long and abbreviation */
                        case 'A':
                                printf("%s", day_long[dt->week_day]);
                                break;
                        case 'a':
                                printf("%s", day_short[dt->week_day]);
                                break;
                        /* Season in long and abbreviation */
                        case 'B':
                                printf("%s", season_long[dt->season]);
                                break;
                        case 'b':
                                printf("%s", season_short[dt->season]);
                                break;
                        /* Day of the season (ordinal and cardinal) */
                        case 'd':
                                printf("%d", dt->season_day + 1);
                                break;
                        case 'e':
                                printf("%d", dt->season_day + 1);
                                switch (dt->season_day) {
                                        case 0:
                                                printf("st");
                                                break;
                                        case 1:
                                                printf("nd");
                                                break;
                                        case 2:
                                                printf("rd");
                                                break;
                                        default:
                                                printf("th");
                                                break;
                                }
                                break;
                        /* YOLD */
                        case 'Y':
                                printf("%d", dt->year);
                                break;
                        /* Holidays */
                        case 'H':
                                if (dt->season_day == 4) {
                                        printf(holidays[dt->season]);
                                }
                                if (dt->season_day == 49) {
                                        printf(holidays[dt->season + 5]);
                                }
                                break;
                        /* Stop parsing the format string, except on Holidays */
                        case 'N':
                                if (dt->season_day != 4 && dt->season_day != 49) {
                                        return 0;
                                }
                                break;
                        /* Newline- and Tabbing-characters */
                        case 'n':
                                printf("\n");
                                break;
                        case 't':
                                printf("\t");
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
                                        printf("St. Tib's Day");
                                } else {
                                        /* ...or parse the substring between %{ and %} ... */
                                        *i = '\0';
                                        if (!format_output(i + 2, dt)) return 0;
                                        *i = '%';
                                }
                                /* ...and continue with the rest */
                                i = tibs_end + 2;
                                break;
                        case '}':
                                i++;
                                break;
                        default:
                                /* No escape-sequence, so we just skip */
                                printf("%%%c",*(i+1));
                                break;
                }
                i++;
        }
        return 1;
}

/* Get the current date and convert it to discordian */
struct disc_time *get_ddate() {
        time_t current_time;
        struct tm *current_tm;
        static struct disc_time dt;

        if ((current_time = time(NULL)) == (time_t)-1)
                return NULL;

        if ((current_tm = localtime(&current_time)) == NULL)
                return NULL;

        /* We have to know, whether we have to insert St. Tib's Day, so whether it's a leap
           year in gregorian calendar */
        int is_leap_year = !(current_tm->tm_year % 4) &&
                            (!(current_tm->tm_year % 400) || current_tm->tm_year % 100);

        if (is_leap_year && current_tm->tm_yday == 59) {
                /* On St. Tibs Day we don't have to define a date */
                dt.st_tibs_day = 1;
        } else {
                dt.st_tibs_day = 0;
                dt.season_day = current_tm->tm_yday % 73;
                if (is_leap_year && current_tm->tm_yday > 59) {
                        dt.week_day = (current_tm->tm_yday - 1) % 5;
                } else {
                        dt.week_day = current_tm->tm_yday % 5;
                }
        }
        dt.year = current_tm->tm_year + 3066;
        dt.season = current_tm->tm_yday / 73;
        return &dt;
}

void print_ddate(const char *format) {
        static char *form = NULL;
        struct disc_time *dt;
        if ((dt = get_ddate()) == NULL)
                return;
        if (form == NULL)
                if ((form = malloc(strlen(format) + 1)) == NULL)
                        return;
        strcpy(form, format);
        format_output(form, dt);
}
