#ifndef _I3STATUS_H
#define _I3STATUS_H

#include <stdbool.h>

#include "queue.h"

#ifdef DZEN
	#define BAR "^fg(#333333)^p(5;-2)^ro(2)^p()^fg()^p(5)"
#elif XMOBAR
	#define BAR "<fc=#333333> | </fc>"
#endif
#define BEGINS_WITH(haystack, needle) (strncmp(haystack, needle, strlen(needle)) == 0)
#define max(a, b) (a > b ? a : b)

#define generate(orderidx, name, function) \
        do { \
                write_to_statusbar(order_to_str(order[orderidx], name), function, (j == (highest_order-1))); \
        } while (0)

#define generate_order(condition, orderidx, name, function) \
        do { \
                if (j == order[orderidx] && condition) \
                        generate(orderidx, name, function); \
        } while (0)

#if defined(LINUX)

#define THERMAL_ZONE "/sys/class/thermal/thermal_zone%d/temp"

#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)

#define THERMAL_ZONE "hw.acpi.thermal.tz%d.temperature"
#define BATT_LIFE "hw.acpi.battery.life"
#define BATT_TIME "hw.acpi.battery.time"
#define BATT_STATE "hw.acpi.battery.state"

#endif

#if defined(__FreeBSD_kernel__) && defined(__GLIBC__)

#include <sys/stat.h>
#include <sys/param.h>

#endif

typedef enum { CS_DISCHARGING, CS_CHARGING, CS_FULL } charging_status_t;
enum { ORDER_RUN, ORDER_WLAN, ORDER_ETH, ORDER_BATTERY, ORDER_CPU_TEMPERATURE, ORDER_LOAD, ORDER_TIME, ORDER_IPV6, MAX_ORDER };

struct battery {
        char *path;
        /* Use last full capacity instead of design capacity */
        bool use_last_full;
        SIMPLEQ_ENTRY(battery) batteries;
};

/* src/general.c */
char *skip_character(char *input, char character, int amount);
void die(const char *fmt, ...);
void create_file(const char *name);
char *order_to_str(int number, char *name);
void setup(void);
void write_to_statusbar(const char *name, const char *message, bool final_entry);
bool slurp(char *filename, char *destination, int size);

/* src/output.c */
void write_error_to_statusbar(const char *message);
char *color(const char *colorstr);
char *endcolor() __attribute__ ((pure));
void cleanup_rbar_dir();

/* src/config.c */
int load_configuration(const char *configfile);

const char *get_ipv6_addr();
const char *get_battery_info(struct battery *bat);
const char *get_ip_addr();
const char *get_wireless_info();
const char *get_cpu_temperature_info();
const char *get_eth_info();
const char *get_load();
bool process_runs(const char *path);

SIMPLEQ_HEAD(battery_head, battery);
extern struct battery_head batteries;

/* socket file descriptor for general purposes */
extern int general_socket;

extern int highest_order;

extern const char *wlan_interface;
extern const char *eth_interface;
extern char *wmii_path;
extern const char *time_format;
extern bool use_colors;
extern bool get_ethspeed;
extern bool get_ipv6;
extern bool get_cpu_temperature;
extern char *thermal_zone;
extern const char *wmii_normcolors;
extern int order[MAX_ORDER];
extern const char **run_watches;
extern unsigned int num_run_watches;
extern unsigned int interval;

#endif
