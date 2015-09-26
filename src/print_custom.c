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
    //Open a pipe with the command
    pipe = popen(args->cmd, "r");

    //fgets is guaranteed to terminate buffer with \000 correctly,  even in the 
    // case that it needs to be cut short due to the `buffer_len` limit
    fgets(args->buffer, sizeof(args->buffer) / sizeof(*(args->buffer)), pipe);

    //Being proper and closing the opened pipe
    pclose(pipe);

    //Always lock the mutex before calling cond_wait otherwise undefined behavior may occur
    pthread_mutex_lock(&args->sleep_mutex);

    //Wait until the sleep_cond is unblocked by a broadcast call called in the main i3status loop
    // Internally, this function unlocks the mutex while waiting for the cond signal. Since it is unlocked,
    // that allows the cond_broadcast containing thread to lock the mutex and broadcast the cond signal needed
    // to unblock the cond_wait. After cond_wait exits, it locks the mutex again. 
    pthread_cond_wait(&args->sleep_cond, &args->sleep_mutex);

    //Then, unlock the mutex directly after as having a locked mutex from cond_wait as leaving the 
    // mutex locked will stall both this thread when the for loop re-loops and reaches the mutex_lock
    // and the main thread containing the cond_broadcast when it re-loops and reaches its mutex_lock
    pthread_mutex_unlock(&args->sleep_mutex);
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
