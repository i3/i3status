const char *wlan_interface = "wlan0";
const char *eth_interface = "eth0";
/* wmii_path has to be terminated by slash */
const char *wmii_path = "/mnt/wmii/rbar/";
const char *time_format = "%d.%m.%Y %H:%M:%S";
const char *battery = "/sys/class/power_supply/BAT0/uevent";
const char *run_watches[] = {"DHCP", "/var/run/dhclient.pid",
			     "VPN", "/var/run/vpnc*.pid"};

#define ORDER_WLAN 	"0"
#define ORDER_ETH 	"1"
#define ORDER_BATTERY 	"2"
#define ORDER_LOAD 	"3"
#define ORDER_TIME 	"4"
