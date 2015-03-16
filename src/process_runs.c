// vim:ts=4:sw=4:expandtab
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
 * If multiple files match the glob pattern, all of them will be checked until
 * the first running process is found.
 *
 */
bool process_runs(const char *path) {
    static char pidbuf[16];
    static glob_t globbuf;
    memset(pidbuf, 0, sizeof(pidbuf));

    if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
        die("glob() failed\n");
    if (globbuf.gl_pathc == 0) {
        /* No glob matches, the specified path does not contain a wildcard. */
        globfree(&globbuf);
        if (!slurp(path, pidbuf, sizeof(pidbuf)))
            return false;
        return (kill(strtol(pidbuf, NULL, 10), 0) == 0 || errno == EPERM);
    }
    for (size_t i = 0; i < globbuf.gl_pathc; i++) {
        if (!slurp(globbuf.gl_pathv[i], pidbuf, sizeof(pidbuf))) {
            globfree(&globbuf);
            return false;
        }
        if (kill(strtol(pidbuf, NULL, 10), 0) == 0 || errno == EPERM) {
            globfree(&globbuf);
            return true;
        }
    }
    globfree(&globbuf);

    return false;
}
