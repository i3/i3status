#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const char *wlan_interface = "wlan0";
const char *eth_interface = "eth0";

// TODO: run-watches, z.B.
// "/var/run/dhcp*.pid" --> checkt ob's das file gibt, ob der prozess läuft und zeigt dann yes/no an
// "/var/run/vpnc*.pid"

char output[512];
bool first_push = true;

static char *skip_character(char *input, char character, int amount) {
	char *walk;
	int len = strlen(input),
	    blanks = 0;

	for (walk = input; ((walk - input) < len) && (blanks < amount); walk++)
		if (*walk == character)
			blanks++;

	return (walk == input ? walk : walk-1);
}

static void push_part(const char *input, const int n) {
	if (first_push)
		first_push = false;
	else
		strncpy(output+strlen(output), " | ", strlen(" | "));
	strncpy(output+strlen(output), input, n);
}

/*
 * Get battery information from /sys. Note that it uses the design capacity to calculate the percentage,
 * not the full capacity.
 *
 */
static char *get_battery_info() {
	char buf[1024];
	static char output[512];
	char *walk, *last = buf;
	int fd = open("/sys/class/power_supply/BAT0/uevent", O_RDONLY);
	int full_design = -1,
	    remaining = -1,
	    present_rate = -1;
	bool charging = false;
	memset(output, '\0', sizeof(output));
	read(fd, buf, sizeof(buf));
	for (walk = buf; (walk-buf) < 1024; walk++)
		if (*walk == '=') {
			if (strncmp(last, "POWER_SUPPLY_ENERGY_FULL_DESIGN", strlen("POWER_SUPPLY_ENERGY_FULL_DESIGN")) == 0)
				full_design = atoi(walk+1);
			else if (strncmp(last, "POWER_SUPPLY_ENERGY_NOW", strlen("POWER_SUPPLY_ENERGY_NOW")) == 0)
				remaining = atoi(walk+1);
			else if (strncmp(last, "POWER_SUPPLY_CURRENT_NOW", strlen("POWER_SUPPLY_CURRENT_NOW")) == 0)
				present_rate = atoi(walk+1);
			else if (strncmp(last, "POWER_SUPPLY_STATUS=Charging", strlen("POWER_SUPPLY_STATUS=Charging")) == 0)
				charging = true;
		} else if (*walk == '\n')
			last = walk+1;
	close(fd);

	if ((full_design != -1) && (remaining != -1) && (present_rate != -1)) {
		float time, perc;
		if (charging)
			time = ((float)full_design - (float)remaining) / (float)present_rate;
		else time = ((float)remaining / (float)present_rate);
		perc = ((float)remaining / (float)full_design);

		int seconds = (int)(time * 3600.0);
		int hours = seconds / 3600;
		seconds -= (hours * 3600);
		int minutes = seconds / 60;
		seconds -= (minutes * 60);

		sprintf(output, "%s %.02f%% %02d:%02d:%02d", (charging ? "CHR" : "BAT"), (perc * 100), hours, minutes, seconds);
	}
	return output;
}

/*
 * Just parses /proc/net/wireless
 *
 */
static char *get_wireless_info() {
	char buf[1024];
	static char output[512];
	char *interfaces;
	memset(buf, '\0', sizeof(buf));
	memset(output, '\0', sizeof(output));

	int fd = open("/proc/net/wireless", O_RDONLY);
	read(fd, buf, sizeof(buf));
	close(fd);

	interfaces = skip_character(buf, '\n', 2) + 1;
	while (interfaces < buf+strlen(buf)) {
		while (isspace((int)*interfaces))
			interfaces++;
		if (strncmp(interfaces, wlan_interface, strlen(wlan_interface)) == 0) {
			/* Skip status field (0000) */
			interfaces += strlen(wlan_interface) + 2;
			interfaces = skip_character(interfaces, ' ', 1);
			while (isspace((int)*interfaces))
				interfaces++;
			int quality = atoi(interfaces);
			/* For some reason, I get 255 sometimes */
			if (quality == 255)
				quality = 0;
			snprintf(output, sizeof(output), "W: (%02d%%) ", quality);
			return output;
		}
		interfaces = skip_character(interfaces, '\n', 1) + 1;
	}

	return output;
}

static char *get_eth_info() {
	static char output[512];
	struct ifreq ifr;
	memset(output, '\0', sizeof(output));

	int fd = socket(AF_INET, SOCK_DGRAM, 0);

	strcpy(ifr.ifr_name, eth_interface);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
		printf("böses fehler\n");
		/* TODO: errorhandling */
	}
	if (!(ifr.ifr_flags & IFF_UP) ||
	    !(ifr.ifr_flags & IFF_RUNNING)) {
		sprintf(output, "E: down");
		close(fd);
		return output;
	}

	strcpy(ifr.ifr_name, eth_interface);
	ifr.ifr_addr.sa_family = AF_INET;
	if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
		struct sockaddr_in addr;
		memcpy(&addr, &ifr.ifr_addr, sizeof(struct sockaddr_in));
		inet_ntop(AF_INET, &addr.sin_addr.s_addr, output+3, sizeof(struct sockaddr_in));
		strncpy(output, "E: ", strlen("E: "));
		if (strlen(output) == 0)
			sprintf(output, "E: no IP");
	}

	close(fd);
	return output;
}

int main() {
	char part[512],
	     *end;

	while (1) {
		memset(output, '\0', sizeof(output));
		first_push = true;

		char *wireless_info = get_wireless_info();
		push_part(wireless_info, strlen(wireless_info));

		char *eth_info = get_eth_info();
		push_part(eth_info, strlen(eth_info));

		char *battery_info = get_battery_info();
		push_part(battery_info, strlen(battery_info));

		
		/* Get load */
		int load_avg = open("/proc/loadavg", O_RDONLY);
		read(load_avg, part, sizeof(part));
		close(load_avg);
		end = skip_character(part, ' ', 3);
		push_part(part, (end-part));

		/* Get date & time */
		time_t current_time = time(NULL);
		struct tm *current_tm = localtime(&current_time);
		strftime(part, sizeof(part), "%d.%m.%Y %H:%M:%S", current_tm);
		push_part(part, strlen(part));

		printf("output = %s\n", output);

		int fd = open("/mnt/wmii/rbar/status", O_RDWR);
		write(fd, output, strlen(output));
		close(fd);

		sleep(1);
	}
}
