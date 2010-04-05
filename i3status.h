#ifndef _I3STATUS_H
#define _I3STATUS_H

enum { O_DZEN2, O_XMOBAR, O_NONE } output_format;

#include <stdbool.h>
#include <confuse.h>

#define BEGINS_WITH(haystack, needle) (strncmp(haystack, needle, strlen(needle)) == 0)
#define max(a, b) ((a) > (b) ? (a) : (b))

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

/* Allows for the definition of a variable without opening a new scope, thus
 * suited for usage in a macro. Idea from wmii. */
#define with(type, var, init) \
        for (type var = (type)-1; (var == (type)-1) && ((var=(init)) || 1); )

#define CASE_SEC(name) \
        if (BEGINS_WITH(current, name)) \
                with(cfg_t *, sec, cfg_getsec(cfg, name)) \
                        if (sec != NULL)

#define CASE_SEC_TITLE(name) \
        if (BEGINS_WITH(current, name)) \
                with(const char *, title, current + strlen(name) + 1) \
                        with(cfg_t *, sec, cfg_gettsec(cfg, name, title)) \
                                if (sec != NULL)


typedef enum { CS_DISCHARGING, CS_CHARGING, CS_FULL } charging_status_t;

/* src/general.c */
char *skip_character(char *input, char character, int amount);
void die(const char *fmt, ...);
bool slurp(char *filename, char *destination, int size);

/* src/output.c */
void print_seperator();
char *color(const char *colorstr);
char *endcolor() __attribute__ ((pure));

void print_ipv6_info(const char *format_up, const char *format_down);
void print_disk_info(const char *path, const char *format);
void print_battery_info(int number, const char *format, bool last_full_capacity);
void print_time(const char *format);
const char *get_ip_addr();
void print_wireless_info(const char *interface, const char *format_up, const char *format_down);
void print_run_watch(const char *title, const char *pidfile, const char *format);
void print_cpu_temperature_info(int zone, const char *format);
void print_eth_info(const char *interface, const char *format_up, const char *format_down);
void print_load();
bool process_runs(const char *path);

/* socket file descriptor for general purposes */
extern int general_socket;

extern cfg_t *cfg, *cfg_general;

#endif
