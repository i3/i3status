#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <glob.h>
#include <unistd.h>

#define _IS_CONFIG_C
#include "config.h"
#undef _IS_CONFIG_C

const char *wlan_interface;
const char *eth_interface;
char *wmii_path;
const char *time_format;
const char *battery_path;
bool use_colors;
bool get_ethspeed;
const char *wmii_normcolors = "#222222 #333333";
char order[MAX_ORDER][2];
const char **run_watches;
unsigned int num_run_watches;
unsigned int interval = 1;

void die(const char *fmt, ...);

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
		die("Could not open configfile");
	char dest_name[512], dest_value[512], whole_buffer[1026];
	struct stat stbuf;
	while (!feof(handle)) {
		char *ret;
		if ((ret = fgets(whole_buffer, 1024, handle)) == whole_buffer) {
			/* sscanf implicitly strips whitespace */
			if (sscanf(whole_buffer, "%s %[^\n]", dest_name, dest_value) < 1)
				continue;
		} else if (ret != NULL)
			die("Could not read line in configuration file");

		/* skip comments and empty lines */
		if (dest_name[0] == '#' || strlen(dest_name) < 3)
			continue;

		OPT("wlan")
			wlan_interface = strdup(dest_value);
		OPT("eth")
			eth_interface = strdup(dest_value);
		OPT("time_format")
			time_format = strdup(dest_value);
		OPT("battery_path")
			battery_path = strdup(dest_value);
		OPT("color")
			use_colors = true;
		OPT("get_ethspeed")
			get_ethspeed = true;
		OPT("normcolors")
			wmii_normcolors = strdup(dest_value);
		OPT("interval")
			interval = atoi(dest_value);
		OPT("wmii_path")
		{
			static glob_t globbuf;
			if (glob(dest_value, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
				die("glob() failed");
			wmii_path = strdup(globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : dest_value);
			globfree(&globbuf);

			if ((stat(wmii_path, &stbuf)) == -1) {
				fprintf(stderr, "Warning: wmii_path contains an invalid path\n");
				free(wmii_path);
				wmii_path = strdup(dest_value);
			}
			if (wmii_path[strlen(wmii_path)-1] != '/')
				die("wmii_path is not terminated by /");
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

	if (wmii_path == NULL)
		exit(EXIT_FAILURE);

	return result;
}
