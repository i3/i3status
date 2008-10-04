#define BEGINS_WITH(haystack, needle) (strncmp(haystack, needle, strlen(needle)) == 0)

static char output[512];
static bool first_push = true;

typedef enum { CS_DISCHARGING, CS_CHARGING, CS_FULL } charging_status_t;

static void write_to_statusbar(const char *message);
static void die(const char *message);
static char *skip_character(char *input, char character, int amount);
static void push_part(const char *input, const int n);
static char *get_battery_info(void);
static char *get_wireless_info(void);
static char *get_ip_address(const char *interface);
static char *get_eth_info(void);
bool process_runs(const char *path);
