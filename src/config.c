#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "i3status.h"

/*
 * Reads the configuration from the given file
 *
 */
int load_configuration(const char *configfile) {
        #define OPT(x) else if (strcasecmp(dest_name, x) == 0)

        /* check if the file exists */
        struct stat buf;
        if (stat(configfile, &buf) < 0)
                return -1;

        int result = 0;
        FILE *handle = fopen(configfile, "r");
        if (handle == NULL)
                die("Could not open configfile\n");
        char dest_name[512], dest_value[512], whole_buffer[1026];

        while (!feof(handle)) {
                char *ret;
                if ((ret = fgets(whole_buffer, 1024, handle)) == whole_buffer) {
                        /* sscanf implicitly strips whitespace */
                        if (sscanf(whole_buffer, "%s %[^\n]", dest_name, dest_value) < 1)
                                continue;
                } else if (ret != NULL)
                        die("Could not read line in configuration file\n");

                /* skip comments and empty lines */
                if (dest_name[0] == '#' || strlen(dest_name) < 3)
                        continue;

                OPT("wlan")
                        wlan_interface = strdup(dest_value);
                OPT("eth")
                        eth_interface = strdup(dest_value);
                OPT("time_format")
                        time_format = strdup(dest_value);
                OPT("battery") {
                        struct battery *new = calloc(1, sizeof(struct battery));
                        if (new == NULL)
                                die("Could not allocate memory\n");
                        if (asprintf(&(new->path), "/sys/class/power_supply/BAT%d/uevent", atoi(dest_value)) == -1)
                                die("Could not build battery path\n");

                        /* check if flags were specified for this battery */
                        if (strstr(dest_value, ",") != NULL) {
                                char *flags = strstr(dest_value, ",");
                                flags++;
                                if (*flags == 'f')
                                        new->use_last_full = true;
                        }
                        SIMPLEQ_INSERT_TAIL(&batteries, new, batteries);
                } OPT("color")
                        use_colors = true;
                OPT("get_ethspeed")
                        get_ethspeed = true;
                OPT("get_cpu_temperature") {
                        get_cpu_temperature = true;
                        if (strlen(dest_value) > 0) {
                                if (asprintf(&thermal_zone, "/sys/class/thermal/thermal_zone%d/temp", atoi(dest_value)) == -1)
                                        die("Could not build thermal_zone path\n");
                        } else {
                                 if (asprintf(&thermal_zone, "/sys/class/thermal/thermal_zone0/temp") == -1)
                                        die("Could not build thermal_zone path\n");
                        }
                } OPT("normcolors")
                        wmii_normcolors = strdup(dest_value);
                OPT("interval")
                        interval = atoi(dest_value);
                OPT("wmii_path")
                {
#ifndef DZEN
                        static glob_t globbuf;
                        struct stat stbuf;
                        if (glob(dest_value, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
                                die("glob() failed\n");
                        wmii_path = strdup(globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : dest_value);
                        globfree(&globbuf);

                        if ((stat(wmii_path, &stbuf)) == -1) {
                                fprintf(stderr, "Warning: wmii_path contains an invalid path\n");
                                free(wmii_path);
                                wmii_path = strdup(dest_value);
                        }
                        if (wmii_path[strlen(wmii_path)-1] != '/')
                                die("wmii_path is not terminated by /\n");
#endif
                }
                OPT("run_watch")
                {
                        char *name = strdup(dest_value);
                        char *path = name;
                        while (*path != ' ')
                                path++;
                        *(path++) = '\0';
                        num_run_watches += 2;
                        run_watches = realloc(run_watches, sizeof(char*) * num_run_watches);
                        run_watches[num_run_watches-2] = name;
                        run_watches[num_run_watches-1] = path;
                }
                OPT("order")
                {
                        #define SET_ORDER(opt, idx) { if (strcasecmp(token, opt) == 0) sprintf(order[idx], "%d", c++); }
                        char *walk, *token;
                        int c = 0;
                        walk = token = dest_value;
                        while (*walk != '\0') {
                                while ((*walk != ',') && (*walk != '\0'))
                                        walk++;
                                *(walk++) = '\0';
                                SET_ORDER("run", ORDER_RUN);
                                SET_ORDER("wlan", ORDER_WLAN);
                                SET_ORDER("eth", ORDER_ETH);
                                SET_ORDER("battery", ORDER_BATTERY);
                                SET_ORDER("cpu_temperature", ORDER_CPU_TEMPERATURE);
                                SET_ORDER("load", ORDER_LOAD);
                                SET_ORDER("time", ORDER_TIME);
                                token = walk;
                                while (isspace((int)(*token)))
                                        token++;
                        }
                }
                else
                {
                        result = -2;
                        die("Unknown configfile option: %s\n", dest_name);
                }
        }
        fclose(handle);

#ifndef DZEN
        if (wmii_path == NULL)
                exit(EXIT_FAILURE);
#endif

        return result;
}
