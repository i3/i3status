// vim:ts=8:expandtab
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>

#include "i3status.h"

/*
 * Writes an errormessage to statusbar
 *
 */
void write_error_to_statusbar(const char *message) {
        cleanup_rbar_dir();
        create_file("error");
        write_to_statusbar("error", message, true);
}

/*
 * Returns the correct color format for dzen (^fg(color)) or wmii (color <normcolors>)
 *
 */
char *color(const char *colorstr) {
        static char colorbuf[32];
#ifdef DZEN
        (void)snprintf(colorbuf, sizeof(colorbuf), "^fg(%s)", colorstr);
#else
        (void)snprintf(colorbuf, sizeof(colorbuf), "%s %s ", colorstr, wmii_normcolors);
#endif
        return colorbuf;
}

/*
 * Cleans wmii's /rbar directory by deleting all regular files
 *
 */
void cleanup_rbar_dir() {
#ifdef DZEN
        return;
#endif
        struct dirent *ent;
        DIR *dir;
        char pathbuf[strlen(wmii_path)+256+1];

        if ((dir = opendir(wmii_path)) == NULL)
                exit(EXIT_FAILURE);

        while ((ent = readdir(dir)) != NULL) {
                if (ent->d_type == DT_REG) {
                        (void)snprintf(pathbuf, sizeof(pathbuf), "%s%s", wmii_path, ent->d_name);
                        if (unlink(pathbuf) == -1)
                                exit(EXIT_FAILURE);
                }
        }

        (void)closedir(dir);
}

/*
 * Creates the specified file in wmii's /rbar directory with
 * correct modes and initializes colors if colormode is enabled
 *
 */
void create_file(const char *name) {
#ifdef DZEN
        return;
#endif
        char pathbuf[strlen(wmii_path)+256+1];
        int fd;
        int flags = O_CREAT | O_WRONLY;
        struct stat statbuf;

        (void)snprintf(pathbuf, sizeof(pathbuf), "%s%s", wmii_path, name);

        /* Overwrite file's contents if it exists */
        if (stat(pathbuf, &statbuf) >= 0)
                flags |= O_TRUNC;

        if ((fd = open(pathbuf, flags, S_IRUSR | S_IWUSR)) < 0)
                exit(EXIT_FAILURE);
        if (use_colors) {
                char *tmp = color("#888888");
                if (write(fd, tmp, strlen(tmp)) != (ssize_t)strlen(tmp))
                        exit(EXIT_FAILURE);
        }
        (void)close(fd);
}

/*
 * Waits until wmii_path/rbar exists (= the filesystem gets mounted),
 * cleans up all files and creates the needed files
 *
 */
void setup(void) {
        unsigned int i;
        char pathbuf[512];

#ifndef DZEN
        struct stat statbuf;
        /* Wait until wmii_path/rbar exists */
        for (; stat(wmii_path, &statbuf) < 0; sleep(interval));
#endif
#define cf(orderidx, name) create_file(order_to_str(order[orderidx], name));

        cleanup_rbar_dir();
        if (wlan_interface)
                cf(ORDER_WLAN, "wlan");
        if (eth_interface)
                cf(ORDER_ETH, "eth");
        if (get_cpu_temperature)
                cf(ORDER_CPU_TEMPERATURE, "cpu_temperature");
        cf(ORDER_LOAD, "load");
        if (time_format)
                cf(ORDER_TIME, "time");
        for (i = 0; i < num_run_watches; i += 2) {
                snprintf(pathbuf, sizeof(pathbuf), "%d%s", order[ORDER_RUN], run_watches[i]);
                create_file(pathbuf);
        }
}

/*
 * Writes the given message in the corresponding file in wmii's /rbar directory
 *
 */
void write_to_statusbar(const char *name, const char *message, bool final_entry) {
#ifdef DZEN
        if (final_entry) {
                if (printf("%s^p(6)\n", message) < 0) {
                        perror("printf");
                        exit(1);
                }

                fflush(stdout);
                return;
        }
        if (printf("%s" BAR, message) < 0) {
                perror("printf");
                exit(1);
        }
        return;
#endif

        char pathbuf[strlen(wmii_path)+256+1];
        int fd;

        (void)snprintf(pathbuf, sizeof(pathbuf), "%s%s", wmii_path, name);
        if ((fd = open(pathbuf, O_RDWR)) == -1) {
                /* Try to re-setup stuff and just continue */
                setup();
                return;
        }
        if (write(fd, message, strlen(message)) != (ssize_t)strlen(message))
                exit(EXIT_FAILURE);
        (void)close(fd);
}
