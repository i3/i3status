/*
 * Generates a status line for use with wmii or other minimal window managers
 *
 *
 * Copyright (c) 2008-2009 Michael Stapelberg and contributors
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
#include <limits.h>
#include <ctype.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <glob.h>
#include <dirent.h>
#include <getopt.h>
#ifdef LINUX
#include <linux/ethtool.h>
#include <linux/sockios.h>
#endif

#define _IS_WMIISTATUS_C
#include "wmiistatus.h"
#undef _IS_WMIISTATUS_C
#include "config.h"

/* socket file descriptor for general purposes */
static int general_socket;

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
 *
 */
static void create_file(const char *name) {
	char pathbuf[strlen(wmii_path)+256+1];
	int fd;
	int flags = O_CREAT | O_WRONLY;
	struct stat statbuf;

	(void)snprintf(pathbuf, sizeof(pathbuf), "%s%s", wmii_path, name);

	/* Overwrite file's contents if it exists */
	if (stat(pathbuf, &statbuf) >= 0)
		flags |= O_TRUNC;

	if ((fd = open(pathbuf, flags, S_IRUSR | S_IWUSR)) < 0)
		exit(EXIT_FAILURE);
	if (use_colors) {
		char *tmp = concat("#888888 ", wmii_normcolors);
		if (write(fd, tmp, strlen(tmp)) != (ssize_t)strlen(tmp))
			exit(EXIT_FAILURE);
	}
	(void)close(fd);
}

/*
 * Waits until wmii_path/rbar exists (= the filesystem gets mounted),
 * cleans up all files and creates the needed files
 *
 */
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
	if (wmii_path != NULL) {
		char buffer[512];
		va_list ap;
		va_start(ap, fmt);
		(void)vsnprintf(buffer, sizeof(buffer), fmt, ap);
		va_end(ap);

		write_error_to_statusbar(buffer);
	}
	exit(EXIT_FAILURE);
}

/*
 * Skip the given character for exactly 'amount' times, returns
 * a pointer to the first non-'character' character in 'input'.
 *
 */
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
 * Get battery information from /sys. Note that it uses the design capacity to
 * calculate the percentage, not the last full capacity, so you can see how
 * worn off your battery is.
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
		return "No battery found";

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
		float remaining_time;
		int seconds, hours, minutes;
		if (status == CS_CHARGING)
			remaining_time = ((float)full_design - (float)remaining) / (float)present_rate;
		else if (status == CS_DISCHARGING)
			remaining_time = ((float)remaining / (float)present_rate);
		else remaining_time = 0;

		seconds = (int)(remaining_time * 3600.0);
		hours = seconds / 3600;
		seconds -= (hours * 3600);
		minutes = seconds / 60;
		seconds -= (minutes * 60);

		(void)snprintf(part, sizeof(part), "%s %.02f%% %02d:%02d:%02d",
			(status == CS_CHARGING ? "CHR" :
			 (status == CS_DISCHARGING ? "BAT" : "FULL")),
			(((float)remaining / (float)full_design) * 100),
			hours, minutes, seconds);
	}
	return part;
}

/*
 * Just parses /proc/net/wireless looking for lines beginning with
 * wlan_interface, extracting the quality of the link and adding the
 * current IP address of wlan_interface.
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

	interfaces = skip_character(buf, '\n', 1) + 1;
	while ((interfaces = skip_character(interfaces, '\n', 1)+1) < buf+strlen(buf)) {
		while (isspace((int)*interfaces))
			interfaces++;
		if (!BEGINS_WITH(interfaces, wlan_interface))
			continue;
		int quality;
		if (sscanf(interfaces, "%*[^:]: 0000 %d", &quality) != 1)
			continue;
		if ((quality == UCHAR_MAX) || (quality == 0)) {
			if (use_colors)
				(void)snprintf(part, sizeof(part), "%s%s",
					concat("#FF0000 ", wmii_normcolors), " W: down");
			else (void)snprintf(part, sizeof(part), "W: down");
		} else (void)snprintf(part, sizeof(part), "W: (%03d%%) %s",
				quality, get_ip_address(wlan_interface));
		return part;
	}

	return part;
}

/*
 * Return the IP address for the given interface or "no IP" if the
 * interface is up and running but hasn't got an IP address yet
 *
 */
static const char *get_ip_address(const char *interface) {
	static char part[512];
	struct ifreq ifr;
	struct sockaddr_in addr;
	socklen_t len = sizeof(struct sockaddr_in);
	memset(part, 0, sizeof(part));

	(void)strcpy(ifr.ifr_name, interface);
	ifr.ifr_addr.sa_family = AF_INET;
	if (ioctl(general_socket, SIOCGIFADDR, &ifr) < 0)
		return NULL;

	memcpy(&addr, &ifr.ifr_addr, len);
	(void)inet_ntop(AF_INET, &addr.sin_addr.s_addr, part, len);
	if (strlen(part) == 0)
		(void)snprintf(part, sizeof(part), "no IP");

	return part;
}

/*
 * Combines ethernet IP addresses and speed (if requested) for displaying
 *
 */
static char *get_eth_info() {
	static char part[512];
	const char *ip_address = get_ip_address(eth_interface);
	int ethspeed = 0;

	if (get_ethspeed) {
#ifdef LINUX
		/* This code path requires root privileges */
		struct ifreq ifr;
		struct ethtool_cmd ecmd;

		ecmd.cmd = ETHTOOL_GSET;
		(void)memset(&ifr, 0, sizeof(ifr));
		ifr.ifr_data = (caddr_t)&ecmd;
		(void)strcpy(ifr.ifr_name, eth_interface);
		if (ioctl(general_socket, SIOCETHTOOL, &ifr) == 0)
			ethspeed = (ecmd.speed == USHRT_MAX ? 0 : ecmd.speed);
		else get_ethspeed = false;
#endif
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
	     procbuf[512];
	static glob_t globbuf;
	struct stat statbuf;
	int fd;
	memset(pidbuf, 0, sizeof(pidbuf));

	if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
		die("glob() failed");
	fd = open((globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : path), O_RDONLY);
	globfree(&globbuf);
	if (fd < 0)
		return false;
	(void)read(fd, pidbuf, sizeof(pidbuf));
	(void)close(fd);

	(void)snprintf(procbuf, sizeof(procbuf), "/proc/%ld", strtol(pidbuf, NULL, 10));
	return (stat(procbuf, &statbuf) >= 0);
}

int main(int argc, char *argv[]) {
	char part[512],
	     pathbuf[512];
	unsigned int i;
	int load_avg;

	char *configfile = PREFIX "/etc/wmiistatus.conf";
	int o, option_index = 0;
	struct option long_options[] = {
		{"config", required_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	while ((o = getopt_long(argc, argv, "c:h", long_options, &option_index)) != -1)
		if ((char)o == 'c')
			configfile = optarg;
		else if ((char)o == 'h') {
			printf("wmiistatus (c) 2008-2009 Michael Stapelberg\n"
				"Syntax: %s [-c <configfile>]\n", argv[0]);
			return 0;
		}

	if (load_configuration(configfile) < 0)
		return EXIT_FAILURE;

	setup();

	if ((general_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		die("Could not create socket");

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
		*skip_character(part, ' ', 3) = '\0';
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
