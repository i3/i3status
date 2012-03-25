// vim:ts=8:expandtab
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <yajl/yajl_gen.h>

#include "i3status.h"

void print_time(yajl_gen json_gen, char *buffer, const char *format, struct tm *current_tm) {
        char *outwalk = buffer;
        if (current_tm == NULL)
                return;
        /* Get date & time */
        outwalk += strftime(outwalk, 4095, format, current_tm);
        *outwalk = '\0';
        OUTPUT_FULL_TEXT(buffer);
}
