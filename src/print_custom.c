// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

void print_custom(yajl_gen json_gen, char *buffer, size_t buffer_len, const char *cmd)
{
/*char *outwalk = buffer;
    struct tm tm;

    if (title != NULL)
        INSTANCE(title);

    //Convert time and format output.
    set_timezone(tz);
    localtime_r(&t, &tm);
    outwalk += strftime(outwalk, 4095, format, &tm);
    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);*/

/* TODO:  I do not think that custom blocks need titles, so they can be removed
 Also, make this section of the code use the macros as above to follow with the custom they use
 Also, comment a bit, note that buf is safe from overflow as fgets is cognizant of buffer sizes as per the 2nd arg*/

  FILE *pipe;
  pipe = popen(cmd, "r");

  fgets(buffer, buffer_len, pipe);
  printf("%s\n", buffer);

  pclose(pipe);
}
