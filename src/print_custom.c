// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

void *custom_thread_fn(void *_args)
{
  //Convert the void type back into a custom_args_t type
  custom_args_t *args = _args;

  FILE *pipe;

  for(;;)
  {
    pipe = popen(args->cmd, "r");

    //fgets is guaranteed to terminate buffer with \000 correctly,  even in the 
    // case that it needs to be cut short due to the `buffer_len` limit
    fgets(args->buffer, sizeof(args->buffer) / sizeof(*(args->buffer)), pipe);

    //Being proper and closing the opened pipe
    pclose(pipe);
  }

  return NULL;
}

void print_custom(yajl_gen json_gen, const char *title, custom_args_t *container)
{
  if(title != NULL)
    INSTANCE(title);

  //It is unknown how much data is written into buffer. Therefore, in order to prevent
  // a premature capping of outwalk in OUTPUT_FULL_TEXT, simply increase the
  // pointer to its last allocated byte
  const size_t buffer_len = sizeof(container->buffer) / sizeof(*(container->buffer));
  char *outwalk = container->buffer + buffer_len - 1;
  OUTPUT_FULL_TEXT(container->buffer);
}
