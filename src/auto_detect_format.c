/*
 * vim:ts=4:sw=4:expandtab
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include "i3status.h"

/*
 * This function tries to automatically find out where i3status is being piped
 * to and choses the appropriate output format.
 *
 * It is a little hackish but should work for most setups :).
 *
 * By iterating through /proc/<number>/stat and finding out the parent process
 * id (just like pstree(1) or ps(1) work), we can get all children of our
 * parent. When the output of i3status is being piped somewhere, the shell
 * (parent process) spawns i3status and the destination process, so we will
 * find our own process and the pipe target.
 *
 * We then check whether the pipe target’s name is known and chose the format.
 *
 */
char *auto_detect_format() {
    pid_t myppid = getppid();
    pid_t mypid = getpid();

    DIR *dir;
    struct dirent *entry;
    char path[255];
    /* the relevant contents (for us) are:
     * <pid> (<program name>) <status> <ppid>
     * which should well fit into one page of 4096 bytes */
    char buffer[4096];

    char *format = NULL;

    char *parentname = NULL;

    if (!(dir = opendir("/proc")))
        return NULL;

    /* First pass: get the executable name of the parent.
     * Upon error, we directly return NULL as we cannot continue without the
     * name of our parent process. */
    while ((entry = readdir(dir)) != NULL) {
        pid_t pid = (pid_t)atoi(entry->d_name);
        if (pid != myppid)
            continue;

        if (snprintf(path, sizeof(path), "/proc/%d/stat", pid) == -1 ||
            !slurp(path, buffer, 4095))
            goto out;

        buffer[4095] = '\0';
        char *leftbracket = strchr(buffer, '(');
        char *rightbracket = strrchr(buffer, ')');
        if (!leftbracket ||
            !rightbracket ||
            !(parentname = malloc((rightbracket - leftbracket))))
            goto out;
        *rightbracket = '\0';
        strcpy(parentname, leftbracket + 1);
    }

    if (!parentname)
        goto out;

    /* Some shells, for example zsh, open a pipe in a way which will make the
     * pipe target the parent process of i3status. If we detect that, we set
     * the format and we are done. */
    if (strcasecmp(parentname, "i3bar") == 0)
        format = "i3bar";
    else if (strcasecmp(parentname, "dzen2") == 0)
        format = "dzen2";
    else if (strcasecmp(parentname, "xmobar") == 0)
        format = "xmobar";

    if (format)
        goto out;

    rewinddir(dir);

    while ((entry = readdir(dir)) != NULL) {
        pid_t pid = (pid_t)atoi(entry->d_name);
        if (pid == 0 || pid == mypid)
            continue;

        if (snprintf(path, sizeof(path), "/proc/%d/stat", pid) == -1)
            continue;

        char *name = NULL;
        pid_t ppid;
        int loopcnt = 0;
        /* Now we need to find out the name of the process.
         * To avoid the possible race condition of the process existing already
         * but not executing the destination (shell after fork() and before
         * exec()), we check if the name equals its parent.
         *
         * We try this for up to 0.5 seconds, then we give up.
         */
        do {
            /* give the scheduler a chance between each iteration, don’t hog
             * the CPU too much */
            if (name)
                usleep(50);

            if (!slurp(path, buffer, 4095))
                break;
            buffer[4095] = '\0';
            char *leftbracket = strchr(buffer, '(');
            char *rightbracket = strrchr(buffer, ')');
            if (!leftbracket ||
                !rightbracket ||
                sscanf(rightbracket + 2, "%*c %d", &ppid) != 1 ||
                ppid != myppid)
                break;
            *rightbracket = '\0';
            name = leftbracket + 1;
        } while (strcmp(parentname, name) == 0 && loopcnt++ < 10000);

        if (!name)
            continue;

        /* Check for known destination programs and set format */
        char *newfmt = NULL;
        if (strcasecmp(name, "i3bar") == 0)
            newfmt = "i3bar";
        else if (strcasecmp(name, "dzen2") == 0)
            newfmt = "dzen2";
        else if (strcasecmp(name, "xmobar") == 0)
            newfmt = "xmobar";

        if (newfmt && format) {
            fprintf(stderr, "i3status: cannot auto-configure, situation ambiguous (format \"%s\" *and* \"%s\" detected)\n", newfmt, format);
            format = NULL;
            break;
        } else {
            format = newfmt;
        }
    }

out:
    if (parentname)
        free(parentname);

    closedir(dir);

    return format;
}
