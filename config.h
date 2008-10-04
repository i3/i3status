const char *wlan_interface = "wlan0";
const char *eth_interface = "eth0";
const char *wmii_path = "/mnt/wmii/rbar/status";
const char *run_watches[] = {"DHCP", "/var/run/dhclient*.pid",
			     "VPN", "/var/run/vpnc*.pid"};
