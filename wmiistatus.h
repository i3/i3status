#define BEGINS_WITH(haystack, needle) (strncmp(haystack, needle, strlen(needle)) == 0)

typedef enum { CS_DISCHARGING, CS_CHARGING, CS_FULL } charging_status_t;

#ifdef _IS_WMIISTATUS_C
static char *concat(const char *str1, const char *str2);
static void cleanup_rbar_dir(void);
static void write_to_statusbar(const char *name, const char *message);
static void write_error_to_statusbar(const char *message);
static char *skip_character(char *input, char character, int amount);
static char *get_battery_info(void);
static char *get_wireless_info(void);
static char *get_ip_address(const char *interface);
static char *get_eth_info(void);
static bool process_runs(const char *path);
#endif

void die(const char *fmt, ...);
