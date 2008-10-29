/*
 * Generates a status line for use with wmii or other minimal window managers
 *
 *
 * Copyright (c) 2008 Michael Stapelberg and contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or other
 *   materials provided with the distribution.
 *
 * * Neither the name of Michael Stapelberg nor the names of contributors
 *   may be used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <glob.h>
#include <dirent.h>
#include <getopt.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>


#define _IS_WMIISTATUS_C
#include "wmiistatus.h"
#undef _IS_WMIISTATUS_C
#include "config.h"

/*
 * This function just concats two strings in place, it should only be used
 * for concatting order to the name of a file or concatting color codes.
 * Otherwise, the buffer size would have to be increased.
 *
 */
static char *concat(const char *str1, const char *str2) {
	static char concatbuf[32];
	(void)snprintf(concatbuf, sizeof(concatbuf), "%s%s", str1, str2);
	return concatbuf;
}

/*
 * Cleans wmii's /rbar directory by deleting all regular files
 *
 */
static void cleanup_rbar_dir() {
	struct dirent *ent;
	DIR *dir;
	char pathbuf[strlen(wmii_path)+256+1];

	if ((dir = opendir(wmii_path)) == NULL)
		exit(EXIT_FAILURE);

	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_type == DT_REG) {
			(void)snprintf(pathbuf, sizeof(pathbuf), "%s%s", wmii_path, ent->d_name);
			if (unlink(pathbuf) == -1)
				exit(EXIT_FAILURE);
		}
	}

	(void)closedir(dir);
}

/*
 * Creates the specified file in wmii's /rbar directory with
 * correct modes and initializes colors if colormode is enabled
 * '
 */
static void create_file(const char *name) {
	char pathbuf[strlen(wmii_path)+256+1];
	int fd;

	(void)snprintf(pathbuf, sizeof(pathbuf), "%s%s", wmii_path, name);
	if ((fd = open(pathbuf, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)) < 0)
		exit(EXIT_FAILURE);
	if (use_colors) {
		char *tmp = concat("#888888 ", wmii_normcolors);
		if (write(fd, tmp, strlen(tmp)) != (ssize_t)strlen(tmp))
			exit(EXIT_FAILURE);
	}
	(void)close(fd);
}

static void setup(void) {
	unsigned int i;
	struct stat statbuf;
	char pathbuf[512];

	/* Wait until wmii_path/rbar exists */
	for (; stat(wmii_path, &statbuf) < 0; sleep(interval));

	cleanup_rbar_dir();
	if (wlan_interface)
		create_file(concat(order[ORDER_WLAN],"wlan"));
	if (eth_interface)
		create_file(concat(order[ORDER_ETH],"eth"));
	if (battery_path)
		create_file(concat(order[ORDER_BATTERY],"battery"));
	create_file(concat(order[ORDER_LOAD],"load"));
	if (time_format)
		create_file(concat(order[ORDER_TIME],"time"));
	for (i = 0; i < num_run_watches; i += 2) {
		snprintf(pathbuf, sizeof(pathbuf), "%s%s", order[ORDER_RUN], run_watches[i]);
		create_file(pathbuf);
	}

}

/*
 * Writes the given message in the corresponding file in wmii's /rbar directory
 *
 */
static void write_to_statusbar(const char *name, const char *message) {
	char pathbuf[strlen(wmii_path)+256+1];
	int fd;

	(void)snprintf(pathbuf, sizeof(pathbuf), "%s%s", wmii_path, name);
	if ((fd = open(pathbuf, O_RDWR)) == -1) {
		/* Try to re-setup stuff and just continue */
		setup();
		return;
	}
	if (write(fd, message, strlen(message)) != (ssize_t)strlen(message))
		exit(EXIT_FAILURE);
	(void)close(fd);
}

/*
 * Writes an errormessage to statusbar
 *
 */
static void write_error_to_statusbar(const char *message) {
	cleanup_rbar_dir();
	create_file("error");
	write_to_statusbar("error", message);
}

/*
 * Write errormessage to statusbar and exit
 *
 */
void die(const char *fmt, ...) {
	char buffer[512];
	va_list ap;
	va_start(ap, fmt);
	(void)vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	if (wmii_path != NULL)
		write_error_to_statusbar(buffer);
	exit(EXIT_FAILURE);
}

static char *skip_character(char *input, char character, int amount) {
	char *walk;
	size_t len = strlen(input);
	int blanks = 0;

	for (walk = input; ((size_t)(walk - input) < len) && (blanks < amount); walk++)
		if (*walk == character)
			blanks++;

	return (walk == input ? walk : walk-1);
}

/*
 * Get battery information from /sys. Note that it uses the design capacity to calculate the percentage,
 * not the full capacity.
 *
 */
static char *get_battery_info() {
	char buf[1024];
	static char part[512];
	char *walk, *last;
	int fd;
	int full_design = -1,
	    remaining = -1,
	    present_rate = -1;
	charging_status_t status = CS_DISCHARGING;

	if ((fd = open(battery_path, O_RDONLY)) == -1)
		die("Could not open %s", battery_path);

	memset(part, 0, sizeof(part));
	(void)read(fd, buf, sizeof(buf));
	for (walk = buf, last = buf; (walk-buf) < 1024; walk++)
		if (*walk == '=') {
			if (BEGINS_WITH(last, "POWER_SUPPLY_ENERGY_FULL_DESIGN") ||
			    BEGINS_WITH(last, "POWER_SUPPLY_CHARGE_FULL_DESIGN"))
				full_design = atoi(walk+1);
			else if (BEGINS_WITH(last, "POWER_SUPPLY_ENERGY_NOW") ||
				 BEGINS_WITH(last, "POWER_SUPPLY_CHARGE_NOW"))
				remaining = atoi(walk+1);
			else if (BEGINS_WITH(last, "POWER_SUPPLY_CURRENT_NOW"))
				present_rate = atoi(walk+1);
			else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Charging"))
				status = CS_CHARGING;
			else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Full"))
				status = CS_FULL;
		} else if (*walk == '\n')
			last = walk+1;
	(void)close(fd);

	if ((full_design != -1) && (remaining != -1) && (present_rate != -1)) {
		float remaining_time, perc;
		int seconds, hours, minutes;
		if (status == CS_CHARGING)
			remaining_time = ((float)full_design - (float)remaining) / (float)present_rate;
		else if (status == CS_DISCHARGING)
			remaining_time = ((float)remaining / (float)present_rate);
		else {
			(void)snprintf(part, sizeof(part), "FULL");
			return part;
		}
		perc = ((float)remaining / (float)full_design);

		seconds = (int)(remaining_time * 3600.0);
		hours = seconds / 3600;
		seconds -= (hours * 3600);
		minutes = seconds / 60;
		seconds -= (minutes * 60);

		(void)snprintf(part, sizeof(part), "%s %.02f%% %02d:%02d:%02d",
			(status == CS_CHARGING? "CHR" : "BAT"),
			(perc * 100), hours, minutes, seconds);
	}
	return part;
}

/*
 * Just parses /proc/net/wireless
 *
 */
static char *get_wireless_info() {
	char buf[1024];
	static char part[512];
	char *interfaces;
	int fd;
	memset(buf, 0, sizeof(buf));
	memset(part, 0, sizeof(part));

	if ((fd = open("/proc/net/wireless", O_RDONLY)) == -1)
		die("Could not open /proc/net/wireless");
	(void)read(fd, buf, sizeof(buf));
	(void)close(fd);

	interfaces = skip_character(buf, '\n', 2) + 1;
	while (interfaces < buf+strlen(buf)) {
		while (isspace((int)*interfaces))
			interfaces++;
		if (strncmp(interfaces, wlan_interface, strlen(wlan_interface)) == 0) {
			int quality;
			/* Skip status field (0000) */
			interfaces += strlen(wlan_interface) + 2;
			interfaces = skip_character(interfaces, ' ', 1);
			while (isspace((int)*interfaces))
				interfaces++;
			quality = atoi(interfaces);
			/* For some reason, I get 255 sometimes */
			if ((quality == 255) || (quality == 0)) {
			if (use_colors)
				(void)snprintf(part, sizeof(part), "%s%s", concat("#FF0000 ", wmii_normcolors), " W: down");
			else (void)snprintf(part, sizeof(part), "W: down");

			} else {
				const char *ip_address;
				(void)snprintf(part, sizeof(part), "W: (%02d%%) ", quality);
				ip_address = get_ip_address(wlan_interface);
				strcpy(part+strlen(part), ip_address);
			}

			return part;
		}
		interfaces = skip_character(interfaces, '\n', 1) + 1;
	}

	return part;
}

static const char *get_ip_address(const char *interface) {
	static char part[512];
	struct ifreq ifr;
	int fd;
	memset(part, 0, sizeof(part));

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	strcpy(ifr.ifr_name, interface);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)
		die("Could not get interface flags (SIOCGIFFLAGS)");

	if (!(ifr.ifr_flags & IFF_UP) ||
	    !(ifr.ifr_flags & IFF_RUNNING)) {
		close(fd);
		return NULL;
	}

	strcpy(ifr.ifr_name, interface);
	ifr.ifr_addr.sa_family = AF_INET;
	if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
		struct sockaddr_in addr;
		memcpy(&addr, &ifr.ifr_addr, sizeof(struct sockaddr_in));
		(void)inet_ntop(AF_INET, &addr.sin_addr.s_addr, part, (socklen_t)sizeof(struct sockaddr_in));
		if (strlen(part) == 0)
			snprintf(part, sizeof(part), "no IP");
	}

	(void)close(fd);
	return part;
}

static char *get_eth_info() {
	static char part[512];
	const char *ip_address = get_ip_address(eth_interface);
	int ethspeed = 0;

	if (get_ethspeed) {
		struct ifreq ifr;
		struct ethtool_cmd ecmd;
		int fd, err;

		if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
			write_error_to_statusbar("Could not open socket");

		ecmd.cmd = ETHTOOL_GSET;
		(void)memset(&ifr, 0, sizeof(ifr));
		ifr.ifr_data = (caddr_t)&ecmd;
		(void)strcpy(ifr.ifr_name, eth_interface);
		if ((err = ioctl(fd, SIOCETHTOOL, &ifr)) == 0)
			ethspeed = ecmd.speed;
		else write_error_to_statusbar("Could not get interface speed. Insufficient privileges?");

		(void)close(fd);
	}

	if (ip_address == NULL)
		(void)snprintf(part, sizeof(part), "E: down");
	else {
		if (get_ethspeed)
			(void)snprintf(part, sizeof(part), "E: %s (%d Mbit/s)", ip_address, ethspeed);
		else (void)snprintf(part, sizeof(part), "E: %s", ip_address);
	}

	return part;
}

/*
 * Checks if the PID in path is still valid by checking if /proc/<pid> exists
 *
 */
static bool process_runs(const char *path) {
	char pidbuf[512],
	     procbuf[512],
	     *walk;
	ssize_t n;
	static glob_t globbuf;
	struct stat statbuf;
	const char *real_path;
	int fd;

	if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
		die("glob() failed");
	real_path = (globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : path);
	fd = open(real_path, O_RDONLY);
	globfree(&globbuf);
	if (fd < 0)
		return false;
	if ((n = read(fd, pidbuf, sizeof(pidbuf))) > 0)
		pidbuf[n] = '\0';
	(void)close(fd);
	for (walk = pidbuf; *walk != '\0'; walk++)
		if (!isdigit((int)(*walk))) {
			*walk = '\0';
			break;
		}
	(void)snprintf(procbuf, sizeof(procbuf), "/proc/%s", pidbuf);
	return (stat(procbuf, &statbuf) >= 0);
}

int main(int argc, char *argv[]) {
	char part[512],
	     pathbuf[512],
	     *end;
	unsigned int i;
	int load_avg;

	char *configfile = PREFIX "/etc/wmiistatus.conf";
	int o, option_index = 0;
	struct option long_options[] = {
		{"config", required_argument, 0, 'c'},
		{0, 0, 0, 0}
	};

	while ((o = getopt_long(argc, argv, "c:", long_options, &option_index)) != -1)
		if ((char)o == 'c')
			configfile = optarg;

	if (load_configuration(configfile) < 0)
		return EXIT_FAILURE;

	setup();

	while (1) {
		for (i = 0; i < num_run_watches; i += 2) {
			bool running = process_runs(run_watches[i+1]);
			if (use_colors)
				snprintf(part, sizeof(part), "%s %s: %s",
					(running ?
						concat("#00FF00 ", wmii_normcolors) :
						concat("#FF0000 ", wmii_normcolors)),
					run_watches[i],
					(running ? "yes" : "no"));
			else snprintf(part, sizeof(part), "%s: %s", run_watches[i], (running ? "yes" : "no"));
			snprintf(pathbuf, sizeof(pathbuf), "%s%s", order[ORDER_RUN], run_watches[i]);
			write_to_statusbar(pathbuf, part);
		}

		if (wlan_interface)
			write_to_statusbar(concat(order[ORDER_WLAN], "wlan"), get_wireless_info());
		if (eth_interface)
			write_to_statusbar(concat(order[ORDER_ETH], "eth"), get_eth_info());
		if (battery_path)
			write_to_statusbar(concat(order[ORDER_BATTERY], "battery"), get_battery_info());

		/* Get load */
		if ((load_avg = open("/proc/loadavg", O_RDONLY)) == -1)
			die("Could not open /proc/loadavg");
		(void)read(load_avg, part, sizeof(part));
		(void)close(load_avg);
		end = skip_character(part, ' ', 3);
		*end = '\0';
		write_to_statusbar(concat(order[ORDER_LOAD], "load"), part);

		if (time_format) {
			/* Get date & time */
			time_t current_time = time(NULL);
			struct tm *current_tm = localtime(&current_time);
			(void)strftime(part, sizeof(part), time_format, current_tm);
			write_to_statusbar(concat(order[ORDER_TIME], "time"), part);
		}

		sleep(interval);
	}
}
