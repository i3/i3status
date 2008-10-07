const char *wlan_interface = "wlan0";
const char *eth_interface = "eth0";
const char *wmii_path = "/mnt/wmii/rbar/status";
const char *time_format = "%d.%m.%Y %H:%M:%S";
const char *battery = "/sys/class/power_supply/BAT0/uevent";
const char *run_watches[] = {"DHCP", "/var/run/dhclient.pid",
			     "VPN", "/var/run/vpnc*.pid"};
