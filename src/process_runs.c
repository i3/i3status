// vim:ts=8:expandtab
#include <stdbool.h>
#include <glob.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef LINUX
/* TODO: correctly check for *BSD */
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#endif

#include "i3status.h"

/*
 * Checks if the PID in path is still valid by checking:
 *  (Linux) if /proc/<pid> exists
 *  (NetBSD) if sysctl returns process infos for this pid
 *
 */
bool process_runs(const char *path) {
        char pidbuf[16];
        static glob_t globbuf;
        int fd;
        memset(pidbuf, 0, sizeof(pidbuf));

        if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
                die("glob() failed\n");
        fd = open((globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : path), O_RDONLY);
        globfree(&globbuf);
        if (fd < 0)
                return false;
        (void)read(fd, pidbuf, sizeof(pidbuf));
        (void)close(fd);

#if defined(LINUX) || defined(__GNU__) || defined(__GLIBC__)
        struct stat statbuf;
        char procbuf[512];
        (void)snprintf(procbuf, sizeof(procbuf), "/proc/%ld", strtol(pidbuf, NULL, 10));
        return (stat(procbuf, &statbuf) >= 0);
#else
        /* TODO: correctly check for NetBSD. Evaluate if this runs on OpenBSD/FreeBSD */
        struct kinfo_proc info;
        size_t length = sizeof(struct kinfo_proc);
        int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, strtol(pidbuf, NULL, 10) };
        if (sysctl(mib, 4, &info, &length, NULL, 0) < 0)
                return false;
        return (length != 0);
#endif
}
