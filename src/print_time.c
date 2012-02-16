// vim:ts=8:expandtab
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "i3status.h"

void print_time(const char *format, struct tm *current_tm) {
        static char part[512];
        /* Get date & time */
        if (current_tm == NULL) {
                return;
        }
        if (output_format == O_I3BAR)
                printf("{\"name\":\"time\", \"full_text\":\"");
        (void)strftime(part, sizeof(part), format, current_tm);
        printf("%s", part);
        if (output_format == O_I3BAR)
                printf("\"}");
}
