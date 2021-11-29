// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <locale.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

static const char uptimefile_linux[] = "/proc/uptime";

void print_uptime(uptime_ctx_t *ctx) {
#if defined(__linux__)
     char *outwalk = ctx->buf;
     
     FILE *file = fopen(uptimefile_linux, "r");

     // bailout if open failed
     if (!file) {
          OUTPUT_FULL_TEXT("can't read uptime");
          fputs("i3status: Cannot read system uptime using /proc/uptime\n", stderr);
          return;
     }
     
     char uptime_buffer[256]; //should be good...
     
     fgets(uptime_buffer, sizeof uptime_buffer, file);
     
     //the first field of /proc/uptime is the uptime in seconds
     long int uptime = strtol(uptime_buffer, NULL, 10);
     
     // stolen from coreutils' uptime.c 
     long int updays = uptime / 86400;
     long int uphours = (uptime - (updays * 86400)) / 3600;
     long int upmins = (uptime - (updays * 86400) - (uphours * 3600)) / 60;
     
     if (0 < updays) {
          outwalk += sprintf(outwalk, "UP: %ld days, %ld:%ld",updays,uphours,upmins);
          *outwalk = '\0';
     } else {
          outwalk += sprintf(outwalk, "UP: %ld:%ld",uphours,upmins);
     }
     
     *outwalk = '\0';
     
     OUTPUT_FULL_TEXT(ctx->buf);
     
     fclose(file);
#else
    OUTPUT_FULL_TEXT("");
    fputs("i3status: Uptime status information is not supported on this system\n", stderr);
#endif
}
