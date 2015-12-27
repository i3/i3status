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
 * Reads /proc/<pid>/stat and returns (via pointers) the name and parent pid of
 * the specified pid.
 * When false is returned, parsing failed and the contents of outname and
 * outpid are undefined.
 *
 */
static bool parse_proc_stat(pid_t pid, char **outname, pid_t *outppid) {
    char path[255];
    /* the relevant contents (for us) are:
     * <pid> (<program name>) <status> <ppid>
     * which should well fit into one page of 4096 bytes */
    char buffer[4096];

    if (snprintf(path, sizeof(path), "/proc/%d/stat", pid) == -1 ||
        !slurp(path, buffer, sizeof(buffer)))
        return false;

    char *leftbracket = strchr(buffer, '(');
    char *rightbracket = strrchr(buffer, ')');
    if (!leftbracket ||
        !rightbracket ||
        sscanf(rightbracket + 2, "%*c %d", outppid) != 1)
        return false;
    *rightbracket = '\0';
    *outname = strdup(leftbracket + 1);
    return true;
}

static char *format_for_process(const char *name) {
    if (strcasecmp(name, "i3bar") == 0 || strcasecmp(name, "swaybar") == 0)
        return "i3bar";
    else if (strcasecmp(name, "dzen2") == 0)
        return "dzen2";
    else if (strcasecmp(name, "xmobar") == 0)
        return "xmobar";
    else
        return NULL;
}

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
char *auto_detect_format(void) {
    /* If stdout is a tty, we output directly to a terminal. */
    if (isatty(STDOUT_FILENO)) {
        return "term";
    }

    pid_t myppid = getppid();
    pid_t mypid = getpid();

    DIR *dir;
    struct dirent *entry;

    char *format = NULL;

    char *parentname;
    pid_t parentpid;

    if (!parse_proc_stat(myppid, &parentname, &parentpid))
        return NULL;

    if (strcmp(parentname, "sh") == 0) {
        pid_t tmp_ppid = parentpid;
        free(parentname);
        fprintf(stderr, "i3status: auto-detection: parent process is \"sh\", looking at its parent\n");
        if (!parse_proc_stat(tmp_ppid, &parentname, &parentpid))
            return NULL;
    }

    /* Some shells, for example zsh, open a pipe in a way which will make the
     * pipe target the parent process of i3status. If we detect that, we set
     * the format and we are done. */
    if ((format = format_for_process(parentname)) != NULL)
        goto out;

    if (!(dir = opendir("/proc")))
        goto out;

    while ((entry = readdir(dir)) != NULL) {
        pid_t pid = (pid_t)atoi(entry->d_name);
        if (pid == 0 || pid == mypid)
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
            if (name) {
                usleep(50);
                free(name);
            }

            if (!parse_proc_stat(pid, &name, &ppid))
                break;
            if (ppid != myppid)
                break;
        } while (strcmp(parentname, name) == 0 && loopcnt++ < 10000);

        if (!name)
            continue;

        /* Check for known destination programs and set format */
        char *newfmt = format_for_process(name);
        free(name);

        if (newfmt && format && strcmp(newfmt, format) != 0) {
            fprintf(stderr, "i3status: cannot auto-configure, situation ambiguous (format \"%s\" *and* \"%s\" detected)\n", newfmt, format);
            format = NULL;
            break;
        } else {
            format = newfmt;
        }
    }

    closedir(dir);

out:
    if (parentname)
        free(parentname);

    return format;
}
