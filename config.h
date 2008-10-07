const char *wlan_interface = "wlan0";
const char *eth_interface = "eth0";
/* wmii_path has to be terminated by slash */
const char *wmii_path = "/mnt/wmii/rbar/";
const char *time_format = "%d.%m.%Y %H:%M:%S";
const char *battery = "/sys/class/power_supply/BAT0/uevent";
const char *run_watches[] = {"DHCP", "/var/run/dhclient.pid",
			     "VPN", "/var/run/vpnc*.pid"};

#define ORDER_RUN	"0"
#define ORDER_WLAN 	"1"
#define ORDER_ETH 	"2"
#define ORDER_BATTERY 	"3"
#define ORDER_LOAD 	"4"
#define ORDER_TIME 	"5"

#define USE_COLORS
