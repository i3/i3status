// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

void print_custom(yajl_gen json_gen, char *buffer, size_t buffer_len, const char *title, const char *cmd)
{
  char *outwalk = buffer;

  if(title != NULL)
    INSTANCE(title);

  FILE *pipe;
  pipe = popen(cmd, "r");

  //fgets is guaranteed to terminate buffer with \000 correctly,  even in the 
  // case that it needs to be cut short due to the `buffer_len` limit
  fgets(buffer, buffer_len, pipe);

  //It is unknown how much data is written into buffer. Therefore, in order to prevent
  // a premature capping of outwalk in OUTPUT_FULL_TEXT, simply increase the
  // pointer to its last allocated byte
  outwalk += buffer_len - 1;
  OUTPUT_FULL_TEXT(buffer);

  //Being proper and closing the opened pipe
  pclose(pipe);
}
