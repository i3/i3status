// vim:ts=8:expandtab
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

void print_time(const char *format, struct tm *current_tm) {
        static char part[512];
        /* Get date & time */
        if (current_tm == NULL) {
                return;
        }
        (void)strftime(part, sizeof(part), format, current_tm);
        printf("%s", part);
}
