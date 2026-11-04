// vim:ts=4:sw=4:expandtab
#include <config.h>
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
 * Returns whether the process whose PID is stored in the given buffer exists.
 *
 * kill() with signal 0 does not do anything: it will return ESRCH if the
 * process does not exist and 0 or EPERM (depending on the uid) if it exists.
 *
 * PIDs that are not positive are rejected up front, because kill() gives them
 * a special meaning (0: our own process group, -1: every process we may
 * signal), which would make a garbled or empty PID file look like a running
 * process.
 *
 */
static bool pid_buffer_runs(const char *pidbuf) {
    const long pid = strtol(pidbuf, NULL, 10);
    if (pid <= 0)
        return false;
    return (kill((pid_t)pid, 0) == 0 || errno == EPERM);
}

/*
 * Checks if the PID in path is still valid by sending signal 0 (does not do
 * anything).
 *
 * If multiple files match the glob pattern, all of them will be checked until
 * the first running process is found.
 *
 */
bool process_runs(const char *path) {
    static char pidbuf[16];
    glob_t globbuf;
    memset(pidbuf, 0, sizeof(pidbuf));
    memset(&globbuf, 0, sizeof(globbuf));

    if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) != 0)
        die("glob() failed\n");

    bool running = false;
    for (size_t i = 0; i < globbuf.gl_pathc && !running; i++) {
        /* A file we cannot read (e.g. a stale one) must not hide the others. */
        if (!slurp(globbuf.gl_pathv[i], pidbuf, sizeof(pidbuf)))
            continue;
        running = pid_buffer_runs(pidbuf);
    }
    globfree(&globbuf);

    return running;
}
