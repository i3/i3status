#include <stdbool.h>

#ifndef _CONFIG_H
#define _CONFIG_H

enum { ORDER_RUN, ORDER_WLAN, ORDER_ETH, ORDER_BATTERY, ORDER_LOAD, ORDER_TIME, MAX_ORDER };

#ifndef _IS_CONFIG_C /* Definitions for everybody */
extern const char *wlan_interface;
extern const char *eth_interface;
extern const char *wmii_path;
extern const char *time_format;
extern const char *battery_path;
extern const char **run_watches;
extern unsigned int num_run_watches;
extern const char *wmii_normcolors;
extern const char order[MAX_ORDER][2];

extern bool use_colors;
#endif

char *glob_path(const char *path);
int load_configuration(const char *configfile);

#endif
