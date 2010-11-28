// vim:ts=8:expandtab
#include <stdbool.h>
#include <glob.h>
#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include "i3status.h"

/*
 * Checks if the PID in path is still valid by sending signal 0 (does not do
 * anything). kill() will return ESRCH if the process does not exist and 0 or
 * EPERM (depending on the uid) if it exists.
 *
 */
bool process_runs(const char *path) {
        static char pidbuf[16];
        static glob_t globbuf;
        memset(pidbuf, 0, sizeof(pidbuf));

        if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
                die("glob() failed\n");
        if (!slurp((globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : path), pidbuf, sizeof(pidbuf))) {
                globfree(&globbuf);
                return false;
        }
        globfree(&globbuf);

        return (kill(strtol(pidbuf, NULL, 10), 0) == 0 || errno == EPERM);
}
