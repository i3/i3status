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
const char *wmii_path;
const char *time_format;
const char *battery_path;
bool use_colors;
const char *wmii_normcolors = "#222222 #333333";
char order[MAX_ORDER][2];
const char **run_watches;
unsigned int num_run_watches;
int interval = 1;

void die(const char *fmt, ...);

/*
 * This function exists primarily for resolving ~ in pathnames. However, you
 * can also specify ~/Movies/ *, which will only return the first match!
 *
 */
char *glob_path(const char *path) {
	static glob_t globbuf;
	if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
		die("glob() failed");
	char *result = strdup(globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : path);
	globfree(&globbuf);
	return result;
}

/*
 * Loads configuration from configfile
 *
 */
static void get_next_config_entry(FILE *handle, char **dest_name, char **dest_value, char *whole_buffer, int whole_buffer_size) {
	char *ret;
	if ((ret = fgets(whole_buffer, whole_buffer_size, handle)) == whole_buffer) {
		char *c = whole_buffer;
		/* Skip whitespaces in the beginning */
		while (isspace((int)*c) && *c != '\0')
			c++;
		*dest_name = c;
		while (!isspace((int)*c))
			c++;
		/* Terminate string as soon as whitespaces begin or it's terminated anyway */
		*(c++) = '\0';

		/* Same for the value: strip whitespaces */
		while (isspace((int)*c) && *c != '\0')
			c++;
		*dest_value = c;
		/* Whitespace is allowed, newline/carriage return is not */
		while ((*c != '\n') && (*c != '\r') && (*c != '\0'))
			c++;
		*c = 0;
	} else if (ret != NULL)
		die("Could not read line in configuration file");
}

/*
 * Reads the configuration from the given file
 *
 */
int load_configuration(const char *configfile) {
	#define OPT(x) else if (strcasecmp(dest_name, x) == 0)

	/* Check if the file exists */
	struct stat buf;
	if (stat(configfile, &buf) < 0)
		return -1;

	int result = 0;
	FILE *handle = fopen(configfile, "r");
	if (handle == NULL)
		die("Could not open configfile");
	char *dest_name = NULL, *dest_value = NULL, whole_buffer[1026];
	struct stat stbuf;
	while (!feof(handle)) {
		get_next_config_entry(handle, &dest_name, &dest_value, whole_buffer, 1024);
		/* No more entries? We're done! */
		if (dest_name == NULL)
			break;
		/* Skip comments and empty lines */
		if (dest_name[0] == '#' || strlen(dest_name) < 3)
			continue;

		OPT("wlan")
		{
			wlan_interface = strdup(dest_value);
		}
		OPT("eth")
		{
			eth_interface = strdup(dest_value);
		}
		OPT("wmii_path")
		{
			char *globbed = glob_path(dest_value);
			if ((stat(globbed, &stbuf)) == -1)
				die("wmii_path contains an invalid path");
			if (globbed[strlen(globbed)-1] != '/')
				die("wmii_path is not terminated by /");
			wmii_path = globbed;
		}
		OPT("time_format")
		{
			time_format = strdup(dest_value);
		}
		OPT("battery_path")
		{
			if ((stat(dest_value, &stbuf)) == -1)
				die("battery_path contains an invalid path");
			battery_path = strdup(dest_value);
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
		OPT("color")
		{
			use_colors = true;
		}
		OPT("normcolors")
		{
			wmii_normcolors = strdup(dest_value);
		}
		OPT("interval")
		{
			interval = atoi(dest_value);
		}
		else
		{
			result = -2;
			die("Unknown configfile option: %s\n", dest_name);
		}
		dest_name = dest_value = NULL;
	}
	fclose(handle);

	if (wmii_path == NULL)
		die("No wmii_path specified in configuration file");

	return result;
}
