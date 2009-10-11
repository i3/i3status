// vim:ts=8:expandtab
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

void print_time(const char *format) {
        static char part[512];
        /* Get date & time */
        time_t current_time = time(NULL);
        struct tm *current_tm = localtime(&current_time);
        (void)strftime(part, sizeof(part), format, current_tm);
        printf("%s", part);
}
