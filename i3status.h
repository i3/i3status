#define BEGINS_WITH(haystack, needle) (strncmp(haystack, needle, strlen(needle)) == 0)

typedef enum { CS_DISCHARGING, CS_CHARGING, CS_FULL } charging_status_t;
enum { ORDER_RUN, ORDER_WLAN, ORDER_ETH, ORDER_BATTERY, ORDER_CPU_TEMPERATURE, ORDER_LOAD, ORDER_TIME, MAX_ORDER };
