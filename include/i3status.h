#ifndef _I3STATUS_H
#define _I3STATUS_H

typedef enum {
    O_DZEN2,
    O_XMOBAR,
    O_I3BAR,
    O_LEMONBAR,
    O_TERM,
    O_NONE
} output_format_t;
extern output_format_t output_format;

typedef enum {
    M_PANGO,
    M_NONE
} markup_format_t;
extern markup_format_t markup_format;

extern char *pct_mark;

#include <stdbool.h>
#include <confuse.h>
#include <time.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>

#define BEGINS_WITH(haystack, needle) (strncmp(haystack, needle, strlen(needle)) == 0)
#define max(a, b) ((a) > (b) ? (a) : (b))

#define DEFAULT_SINK_INDEX UINT32_MAX
#define COMPOSE_VOLUME_MUTE(vol, mute) ((vol) | ((mute) ? (1 << 30) : 0))
#define DECOMPOSE_VOLUME(cvol) ((cvol) & ~(1 << 30))
#define DECOMPOSE_MUTED(cvol) (((cvol) & (1 << 30)) != 0)
#define MAX_SINK_DESCRIPTION_LEN (128) /* arbitrary */

#if defined(__linux__)

#define THERMAL_ZONE "/sys/class/thermal/thermal_zone%d/temp"

#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)

/* this needs the coretemp module to be loaded */
#if defined(__DragonFly__)
#define THERMAL_ZONE "hw.sensors.cpu%d.temp0"
#else
#define THERMAL_ZONE "dev.cpu.%d.temperature"
#endif
#define BATT_LIFE "hw.acpi.battery.life"
#define BATT_TIME "hw.acpi.battery.time"
#define BATT_STATE "hw.acpi.battery.state"

#elif defined(__OpenBSD__) || defined(__NetBSD__)
/* Default to acpitz(4) if no path is set. */
#define THERMAL_ZONE "acpitz%d"
#endif

#if defined(__FreeBSD_kernel__) && defined(__GLIBC__)

#include <sys/stat.h>
#include <sys/param.h>

#endif

/* Allows for the definition of a variable without opening a new scope, thus
 * suited for usage in a macro. Idea from wmii. */
#define with(type, var, init) \
    for (type var = (type)-1; (var == (type)-1) && ((var = (init)) || 1); var = (type)1)

#define CASE_SEC(name)              \
    if (BEGINS_WITH(current, name)) \
    with(cfg_t *, sec, cfg_section = cfg_getsec(cfg, name)) if (sec != NULL)

#define CASE_SEC_TITLE(name)                              \
    if (BEGINS_WITH(current, name))                       \
    with(const char *, title, current + strlen(name) + 1) \
        with(cfg_t *, sec, cfg_section = cfg_gettsec(cfg, name, title)) if (sec != NULL)

/* Macro which any plugin can use to output the full_text part (when the output
 * format is JSON) or just output to stdout (any other output format). */
#define OUTPUT_FULL_TEXT(text)                                                                       \
    do {                                                                                             \
        /* Terminate the output buffer here in any case, so that it’s                              \
         * not forgotten in the module */                                                            \
        *outwalk = '\0';                                                                             \
        if (output_format == O_I3BAR) {                                                              \
            char *_markup = cfg_getstr(cfg_general, "markup");                                       \
            yajl_gen_string(ctx->json_gen, (const unsigned char *)"markup", strlen("markup"));       \
            yajl_gen_string(ctx->json_gen, (const unsigned char *)_markup, strlen(_markup));         \
            yajl_gen_string(ctx->json_gen, (const unsigned char *)"full_text", strlen("full_text")); \
            yajl_gen_string(ctx->json_gen, (const unsigned char *)text, strlen(text));               \
        } else {                                                                                     \
            printf("%s", text);                                                                      \
        }                                                                                            \
    } while (0)

#define SEC_OPEN_MAP(name)                                                            \
    do {                                                                              \
        if (output_format == O_I3BAR) {                                               \
            yajl_gen_map_open(json_gen);                                              \
            yajl_gen_string(json_gen, (const unsigned char *)"name", strlen("name")); \
            yajl_gen_string(json_gen, (const unsigned char *)name, strlen(name));     \
        }                                                                             \
    } while (0)

#define SEC_CLOSE_MAP                                                                                                       \
    do {                                                                                                                    \
        if (output_format == O_I3BAR) {                                                                                     \
            char *_align = cfg_getstr(sec, "align");                                                                        \
            if (_align) {                                                                                                   \
                yajl_gen_string(json_gen, (const unsigned char *)"align", strlen("align"));                                 \
                yajl_gen_string(json_gen, (const unsigned char *)_align, strlen(_align));                                   \
            }                                                                                                               \
            struct min_width *_width = cfg_getptr(sec, "min_width");                                                        \
            if (_width) {                                                                                                   \
                /* if the value can be parsed as a number, we use the numerical value */                                    \
                if (_width->num > 0) {                                                                                      \
                    yajl_gen_string(json_gen, (const unsigned char *)"min_width", strlen("min_width"));                     \
                    yajl_gen_integer(json_gen, _width->num);                                                                \
                } else {                                                                                                    \
                    yajl_gen_string(json_gen, (const unsigned char *)"min_width", strlen("min_width"));                     \
                    yajl_gen_string(json_gen, (const unsigned char *)_width->str, strlen(_width->str));                     \
                }                                                                                                           \
            }                                                                                                               \
            if (cfg_size(sec, "separator") > 0) {                                                                           \
                yajl_gen_string(json_gen, (const unsigned char *)"separator", strlen("separator"));                         \
                yajl_gen_bool(json_gen, cfg_getbool(sec, "separator"));                                                     \
            }                                                                                                               \
            if (cfg_size(sec, "separator_block_width") > 0) {                                                               \
                yajl_gen_string(json_gen, (const unsigned char *)"separator_block_width", strlen("separator_block_width")); \
                yajl_gen_integer(json_gen, cfg_getint(sec, "separator_block_width"));                                       \
            }                                                                                                               \
            const char *_sep = cfg_getstr(cfg_general, "separator");                                                        \
            if (strlen(_sep) == 0) {                                                                                        \
                yajl_gen_string(json_gen, (const unsigned char *)"separator", strlen("separator"));                         \
                yajl_gen_bool(json_gen, false);                                                                             \
            }                                                                                                               \
            yajl_gen_map_close(json_gen);                                                                                   \
        }                                                                                                                   \
    } while (0)

#define START_COLOR(colorstr)                                                                    \
    do {                                                                                         \
        if (cfg_getbool(cfg_general, "colors")) {                                                \
            const char *_val = NULL;                                                             \
            if (cfg_section)                                                                     \
                _val = cfg_getstr(cfg_section, colorstr);                                        \
            if (!_val)                                                                           \
                _val = cfg_getstr(cfg_general, colorstr);                                        \
            if (output_format == O_I3BAR) {                                                      \
                yajl_gen_string(ctx->json_gen, (const unsigned char *)"color", strlen("color")); \
                yajl_gen_string(ctx->json_gen, (const unsigned char *)_val, strlen(_val));       \
            } else {                                                                             \
                outwalk += sprintf(outwalk, "%s", color(colorstr));                              \
            }                                                                                    \
        }                                                                                        \
    } while (0)

#define END_COLOR                                                             \
    do {                                                                      \
        if (cfg_getbool(cfg_general, "colors") && output_format != O_I3BAR) { \
            outwalk += sprintf(outwalk, "%s", endcolor());                    \
        }                                                                     \
    } while (0)

#define INSTANCE(instance)                                                                         \
    do {                                                                                           \
        if (output_format == O_I3BAR) {                                                            \
            yajl_gen_string(ctx->json_gen, (const unsigned char *)"instance", strlen("instance")); \
            yajl_gen_string(ctx->json_gen, (const unsigned char *)instance, strlen(instance));     \
        }                                                                                          \
    } while (0)

#define OUTPUT_FORMATTED                                             \
    do {                                                             \
        const size_t remaining = ctx->buflen - (outwalk - ctx->buf); \
        strncpy(outwalk, formatted, remaining);                      \
        outwalk += strlen(formatted);                                \
    } while (0)

/*
 * The "min_width" module option may either be defined as a string or a number.
 */
struct min_width {
    long num;
    const char *str;
};

/* src/general.c */
char *skip_character(char *input, char character, int amount);

void die(const char *fmt, ...) __attribute__((format(printf, 1, 2), noreturn));
bool slurp(const char *filename, char *destination, int size);
char *resolve_tilde(const char *path);
void *scalloc(size_t size);
char *sstrdup(const char *str);

/* src/output.c */
void print_separator(const char *separator);
char *color(const char *colorstr);
char *endcolor() __attribute__((pure));
void reset_cursor(void);
void maybe_escape_markup(char *text, char *buffer, size_t size);

char *rtrim(const char *s);
char *ltrim(const char *s);
char *trim(const char *s);

// copied from  i3:libi3/format_placeholders.c
/* src/format_placeholders.c */
typedef struct {
    /* The placeholder to be replaced, e.g., "%title". */
    const char *name;
    /* The value this placeholder should be replaced with. */
    const char *value;
} placeholder_t;
char *format_placeholders(const char *format, placeholder_t *placeholders, int num);

/* src/auto_detect_format.c */
char *auto_detect_format();

/* src/print_time.c */
void set_timezone(const char *tz);

/* src/first_network_device.c */
typedef enum {
    NET_TYPE_WIRELESS = 0,
    NET_TYPE_ETHERNET = 1,
    NET_TYPE_OTHER = 2
} net_type_t;
const char *first_eth_interface(const net_type_t type);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *format_up;
    const char *format_down;
} ipv6_info_ctx_t;

void print_ipv6_info(ipv6_info_ctx_t *ctx);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *path;
    const char *format;
    const char *format_below_threshold;
    const char *format_not_mounted;
    const char *prefix_type;
    const char *threshold_type;
    const double low_threshold;
} disk_info_ctx_t;

void print_disk_info(disk_info_ctx_t *ctx);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    int number;
    const char *path;
    const char *format;
    const char *format_down;
    const char *status_chr;
    const char *status_bat;
    const char *status_unk;
    const char *status_full;
    const char *status_idle;
    int low_threshold;
    char *threshold_type;
    bool last_full_capacity;
    const char *format_percentage;
    bool hide_seconds;
} battery_info_ctx_t;

void print_battery_info(battery_info_ctx_t *ctx);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *title;
    const char *format;
    const char *tz;
    const char *locale;
    const char *format_time;
    bool hide_if_equals_localtime;
    time_t t;
} time_ctx_t;

void print_time(time_ctx_t *ctx);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *format;
    time_t t;
} ddate_ctx_t;

void print_ddate(ddate_ctx_t *ctx);

const char *get_ip_addr(const char *interface, int family);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *interface;
    const char *format_up;
    const char *format_down;
    const char *format_bitrate;
    const char *format_noise;
    const char *format_quality;
    const char *format_signal;
} wireless_info_ctx_t;

void print_wireless_info(wireless_info_ctx_t *ctx);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *title;
    const char *pidfile;
    const char *format;
    const char *format_down;
} run_watch_ctx_t;

void print_run_watch(run_watch_ctx_t *ctx);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *title;
    const char *path;
    const char *format;
    const char *format_down;
} path_exists_ctx_t;

void print_path_exists(path_exists_ctx_t *ctx);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    int zone;
    const char *path;
    const char *format;
    const char *format_above_threshold;
    int max_threshold;
} cpu_temperature_ctx_t;

void print_cpu_temperature_info(cpu_temperature_ctx_t *ctx);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *format;
    const char *format_above_threshold;
    const char *format_above_degraded_threshold;
    const char *path;
    const float max_threshold;
    const float degraded_threshold;
} cpu_usage_ctx_t;

void print_cpu_usage(cpu_usage_ctx_t *ctx);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *interface;
    const char *format_up;
    const char *format_down;
} eth_info_ctx_t;

void print_eth_info(eth_info_ctx_t *ctx);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *format;
    const char *format_above_threshold;
    const float max_threshold;
} load_ctx_t;

void print_load(load_ctx_t *ctx);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *format;
    const char *format_degraded;
    const char *threshold_degraded;
    const char *threshold_critical;
    const char *memory_used_method;
    const char *unit;
    const int decimals;
} memory_ctx_t;

void print_memory(memory_ctx_t *ctx);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *fmt;
    const char *fmt_muted;
    const char *device;
    const char *mixer;
    int mixer_idx;
} volume_ctx_t;

void print_volume(volume_ctx_t *ctx);

bool process_runs(const char *path);
int volume_pulseaudio(uint32_t sink_idx, const char *sink_name);
bool description_pulseaudio(uint32_t sink_idx, const char *sink_name, char buffer[MAX_SINK_DESCRIPTION_LEN]);
bool pulse_initialize(void);

typedef struct {
    yajl_gen json_gen;
    char *buf;
    const size_t buflen;
    const char *title;
    const char *path;
    const char *format;
    const char *format_bad;
    const int max_chars;
} file_contents_ctx_t;

void print_file_contents(file_contents_ctx_t *ctx);

/* socket file descriptor for general purposes */
extern int general_socket;

extern cfg_t *cfg, *cfg_general, *cfg_section;

extern void **cur_instance;

extern pthread_t main_thread;
#endif
