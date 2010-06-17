// vim:ts=8:expandtab
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

void print_time(const char *format) {
        static char part[512];
        /* Get date & time */
        time_t current_time = time(NULL);
        if (current_time == (time_t) -1) {
                return;
        }
        struct tm *current_tm = localtime(&current_time);
        if (current_tm == NULL) {
                return;
        }
        (void)strftime(part, sizeof(part), format, current_tm);
        printf("%s", part);
}
