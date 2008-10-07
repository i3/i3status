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

#include "wmiistatus.h"
#include "config.h"

static void cleanup_rbar_dir() {
	struct dirent *ent;
	DIR *dir;
	char pathbuf[strlen(wmii_path)+256+1];

	if ((dir = opendir(wmii_path)) == NULL)
		exit(-3);

	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_type == DT_REG) {
			sprintf(pathbuf, "%s%s", wmii_path, ent->d_name);
			unlink(pathbuf);
		}
	}

	closedir(dir);
}

static void create_file(const char *name) {
	char pathbuf[strlen(wmii_path)+256+1];

	sprintf(pathbuf, "%s%s", wmii_path, name);
	int fd = creat(pathbuf, S_IRUSR | S_IWUSR);
	if (fd < 0)
		exit(-4);
	close(fd);
}

static void write_to_statusbar(const char *name, const char *message) {
	char pathbuf[strlen(wmii_path)+256+1];

	sprintf(pathbuf, "%s%s", wmii_path, name);
	int fd = open(pathbuf, O_RDWR);
	if (fd == -1)
		exit(-2);
	write(fd, message, strlen(message));
	close(fd);
}

static void write_error_to_statusbar(const char *message) {
	cleanup_rbar_dir();
	write_to_statusbar("error", message);
}

/*
 * Write errormessage to statusbar and exit
 *
 */
static void die(const char *fmt, ...) {
	char buffer[512];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	va_end(ap);

	write_error_to_statusbar(buffer);
	exit(-1);
}

static char *skip_character(char *input, char character, int amount) {
	char *walk;
	int len = strlen(input),
	    blanks = 0;

	for (walk = input; ((walk - input) < len) && (blanks < amount); walk++)
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
	char *walk, *last = buf;
	int fd = open(battery, O_RDONLY);
	if (fd == -1)
		die("Could not open %s", battery);
	int full_design = -1,
	    remaining = -1,
	    present_rate = -1;
	charging_status_t status = CS_DISCHARGING;
	memset(part, '\0', sizeof(part));
	read(fd, buf, sizeof(buf));
	for (walk = buf; (walk-buf) < 1024; walk++)
		if (*walk == '=') {
			if (BEGINS_WITH(last, "POWER_SUPPLY_ENERGY_FULL_DESIGN"))
				full_design = atoi(walk+1);
			else if (BEGINS_WITH(last, "POWER_SUPPLY_ENERGY_NOW"))
				remaining = atoi(walk+1);
			else if (BEGINS_WITH(last, "POWER_SUPPLY_CURRENT_NOW"))
				present_rate = atoi(walk+1);
			else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Charging"))
				status = CS_CHARGING;
			else if (BEGINS_WITH(last, "POWER_SUPPLY_STATUS=Full"))
				status = CS_FULL;
		} else if (*walk == '\n')
			last = walk+1;
	close(fd);

	if ((full_design != -1) && (remaining != -1) && (present_rate != -1)) {
		float remaining_time, perc;
		if (status == CS_CHARGING)
			remaining_time = ((float)full_design - (float)remaining) / (float)present_rate;
		else if (status == CS_DISCHARGING)
			remaining_time = ((float)remaining / (float)present_rate);
		else {
			snprintf(part, sizeof(part), "FULL");
			return part;
		}
		perc = ((float)remaining / (float)full_design);

		int seconds = (int)(remaining_time * 3600.0);
		int hours = seconds / 3600;
		seconds -= (hours * 3600);
		int minutes = seconds / 60;
		seconds -= (minutes * 60);

		sprintf(part, "%s %.02f%% %02d:%02d:%02d", (status == CS_CHARGING? "CHR" : "BAT"),
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
	memset(buf, '\0', sizeof(buf));
	memset(part, '\0', sizeof(part));

	int fd = open("/proc/net/wireless", O_RDONLY);
	if (fd == -1)
		die("Could not open /proc/net/wireless");
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
			if ((quality == 255) || (quality == 0))
				snprintf(part, sizeof(part), "W: down");
			else {
				snprintf(part, sizeof(part), "W: (%02d%%) ", quality);
				char *ip_address = get_ip_address(wlan_interface);
				strcpy(part+strlen(part), ip_address);
			}

			return part;
		}
		interfaces = skip_character(interfaces, '\n', 1) + 1;
	}

	return part;
}

static char *get_ip_address(const char *interface) {
	static char part[512];
	struct ifreq ifr;
	memset(part, '\0', sizeof(part));

	int fd = socket(AF_INET, SOCK_DGRAM, 0);

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
		inet_ntop(AF_INET, &addr.sin_addr.s_addr, part, sizeof(struct sockaddr_in));
		if (strlen(part) == 0)
			sprintf(part, "no IP");
	}

	close(fd);
	return part;
}

static char *get_eth_info() {
	static char part[512];
	char *ip_address = get_ip_address(eth_interface);

	if (ip_address == NULL)
		snprintf(part, sizeof(part), "E: down");
	else snprintf(part, sizeof(part), "E: %s", ip_address);

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
	int n;
	static glob_t globbuf;
	struct stat statbuf;

	if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
		die("glob() failed");
	const char *real_path = (globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : path);
	int fd = open(real_path, O_RDONLY);
	globfree(&globbuf);
	if (fd < 0)
		return false;
	n = read(fd, pidbuf, sizeof(pidbuf));
	if (n > 0)
		pidbuf[n] = '\0';
	close(fd);
	for (walk = pidbuf; *walk != '\0'; walk++)
		if (!isdigit((int)(*walk))) {
			*walk = '\0';
			break;
		}
	sprintf(procbuf, "/proc/%s", pidbuf);
	return (stat(procbuf, &statbuf) >= 0);
}

int main(void) {
	char part[512],
	     pathbuf[512],
	     *end;
	unsigned int i;

	cleanup_rbar_dir();
	create_file(ORDER_WLAN "wlan");
	create_file(ORDER_ETH "eth");
	create_file(ORDER_BATTERY "battery");
	create_file(ORDER_LOAD "load");
	create_file(ORDER_TIME "time");
	for (i = 0; i < sizeof(run_watches) / sizeof(char*); i += 2) {
		sprintf(pathbuf, "%s%s", ORDER_RUN, run_watches[i]);
		create_file(pathbuf);
	}

	while (1) {
		for (i = 0; i < sizeof(run_watches) / sizeof(char*); i += 2) {
			sprintf(part, "%s: %s", run_watches[i], (process_runs(run_watches[i+1]) ? "yes" : "no"));
			sprintf(pathbuf, "%s%s", ORDER_RUN, run_watches[i]);
			write_to_statusbar(pathbuf, part);
		}

		write_to_statusbar(ORDER_WLAN "wlan", get_wireless_info());
		write_to_statusbar(ORDER_ETH "eth", get_eth_info());
		write_to_statusbar(ORDER_BATTERY "battery", get_battery_info());

		/* Get load */
		int load_avg = open("/proc/loadavg", O_RDONLY);
		if (load_avg == -1)
			die("Could not open /proc/loadavg");
		read(load_avg, part, sizeof(part));
		close(load_avg);
		end = skip_character(part, ' ', 3);
		*end = '\0';
		write_to_statusbar(ORDER_LOAD "load", part);

		/* Get date & time */
		time_t current_time = time(NULL);
		struct tm *current_tm = localtime(&current_time);
		strftime(part, sizeof(part), time_format, current_tm);
		write_to_statusbar(ORDER_TIME "time", part);

		sleep(1);
	}
}
