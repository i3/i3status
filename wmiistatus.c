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
#else
/* TODO: correctly check for *BSD */
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#endif

#include "wmiistatus.h"

#define BAR "^fg(#333333)^p(5;-2)^ro(2)^p()^fg()^p(5)"

/* socket file descriptor for general purposes */
static int general_socket;

static const char *wlan_interface;
static const char *eth_interface;
static char *wmii_path;
static const char *time_format;
static const char *battery_path;
static bool use_colors;
static bool get_ethspeed;
static const char *wmii_normcolors = "#222222 #333333";
static char order[MAX_ORDER][2];
static const char **run_watches;
static unsigned int num_run_watches;
static unsigned int interval = 1;

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
 * Returns the correct color format for dzen (^fg(color)) or wmii (color <normcolors>)
 *
 */
static char *color(const char *colorstr) {
	static char colorbuf[32];
#ifdef DZEN
	(void)snprintf(colorbuf, sizeof(colorbuf), "^fg(%s)", colorstr);
#else
	(void)snprintf(colorbuf, sizeof(colorbuf), "%s %s ", colorstr, wmii_normcolors);
#endif
	return colorbuf;
}

/*
 * Cleans wmii's /rbar directory by deleting all regular files
 *
 */
static void cleanup_rbar_dir() {
#ifdef DZEN
	return;
#endif
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
#ifdef DZEN
	return;
#endif
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
		char *tmp = color("#888888");
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
	char pathbuf[512];

#ifndef DZEN
	struct stat statbuf;
	/* Wait until wmii_path/rbar exists */
	for (; stat(wmii_path, &statbuf) < 0; sleep(interval));
#endif

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
static void write_to_statusbar(const char *name, const char *message, bool final_entry) {
#ifdef DZEN
	if (final_entry) {
		(void)printf("%s^p(6)\n", message);
		fflush(stdout);
		return;
	}
	(void)printf("%s" BAR, message);
	return;
#endif

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
	write_to_statusbar("error", message, true);
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
		die("Could not open /proc/net/wireless\n");
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
				(void)snprintf(part, sizeof(part), "%sW: down", color("#FF0000"));
			else (void)snprintf(part, sizeof(part), "W: down");
		} else (void)snprintf(part, sizeof(part), "%sW: (%03d%%) %s",
				color("#00FF00"), quality, get_ip_address(wlan_interface));
		return part;
	}

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
 * Checks if the PID in path is still valid by checking:
 *  (Linux) if /proc/<pid> exists
 *  (NetBSD) if sysctl returns process infos for this pid
 *
 */
static bool process_runs(const char *path) {
	char pidbuf[16];
	static glob_t globbuf;
	int fd;
	memset(pidbuf, 0, sizeof(pidbuf));

	if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
		die("glob() failed\n");
	fd = open((globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : path), O_RDONLY);
	globfree(&globbuf);
	if (fd < 0)
		return false;
	(void)read(fd, pidbuf, sizeof(pidbuf));
	(void)close(fd);

#ifdef LINUX
	struct stat statbuf;
	char procbuf[512];
	(void)snprintf(procbuf, sizeof(procbuf), "/proc/%ld", strtol(pidbuf, NULL, 10));
	return (stat(procbuf, &statbuf) >= 0);
#else
	/* TODO: correctly check for NetBSD. Evaluate if this runs on OpenBSD/FreeBSD */
	struct kinfo_proc info;
	size_t length = sizeof(struct kinfo_proc);
	int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, strtol(pidbuf, NULL, 10) };
	if (sysctl(mib, 4, &info, &length, NULL, 0) < 0)
		return false;
	return (length != 0);
#endif
}

/*
 * Reads the configuration from the given file
 *
 */
static int load_configuration(const char *configfile) {
	#define OPT(x) else if (strcasecmp(dest_name, x) == 0)

	/* check if the file exists */
	struct stat buf;
	if (stat(configfile, &buf) < 0)
		return -1;

	int result = 0;
	FILE *handle = fopen(configfile, "r");
	if (handle == NULL)
		die("Could not open configfile\n");
	char dest_name[512], dest_value[512], whole_buffer[1026];

	while (!feof(handle)) {
		char *ret;
		if ((ret = fgets(whole_buffer, 1024, handle)) == whole_buffer) {
			/* sscanf implicitly strips whitespace */
			if (sscanf(whole_buffer, "%s %[^\n]", dest_name, dest_value) < 1)
				continue;
		} else if (ret != NULL)
			die("Could not read line in configuration file\n");

		/* skip comments and empty lines */
		if (dest_name[0] == '#' || strlen(dest_name) < 3)
			continue;

		OPT("wlan")
			wlan_interface = strdup(dest_value);
		OPT("eth")
			eth_interface = strdup(dest_value);
		OPT("time_format")
			time_format = strdup(dest_value);
		OPT("battery_path")
			battery_path = strdup(dest_value);
		OPT("color")
			use_colors = true;
		OPT("get_ethspeed")
			get_ethspeed = true;
		OPT("normcolors")
			wmii_normcolors = strdup(dest_value);
		OPT("interval")
			interval = atoi(dest_value);
		OPT("wmii_path")
		{
#ifndef DZEN
			static glob_t globbuf;
			struct stat stbuf;
			if (glob(dest_value, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
				die("glob() failed\n");
			wmii_path = strdup(globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : dest_value);
			globfree(&globbuf);

			if ((stat(wmii_path, &stbuf)) == -1) {
				fprintf(stderr, "Warning: wmii_path contains an invalid path\n");
				free(wmii_path);
				wmii_path = strdup(dest_value);
			}
			if (wmii_path[strlen(wmii_path)-1] != '/')
				die("wmii_path is not terminated by /\n");
#endif
		}
		OPT("run_watch")
		{
			char *name = strdup(dest_value);
			char *path = name;
			while (*path != ' ')
				path++;
			*(path++) = '\0';
			num_run_watches += 2;
			run_watches = realloc(run_watches, sizeof(char*) * num_run_watches);
			run_watches[num_run_watches-2] = name;
			run_watches[num_run_watches-1] = path;
		}
		OPT("order")
		{
			#define SET_ORDER(opt, idx) { if (strcasecmp(token, opt) == 0) sprintf(order[idx], "%d", c++); }
			char *walk, *token;
			int c = 0;
			walk = token = dest_value;
			while (*walk != '\0') {
				while ((*walk != ',') && (*walk != '\0'))
					walk++;
				*(walk++) = '\0';
				SET_ORDER("run", ORDER_RUN);
				SET_ORDER("wlan", ORDER_WLAN);
				SET_ORDER("eth", ORDER_ETH);
				SET_ORDER("battery", ORDER_BATTERY);
				SET_ORDER("load", ORDER_LOAD);
				SET_ORDER("time", ORDER_TIME);
				token = walk;
				while (isspace((int)(*token)))
					token++;
			}
		}
		else
		{
			result = -2;
			die("Unknown configfile option: %s\n", dest_name);
		}
	}
	fclose(handle);

#ifndef DZEN
	if (wmii_path == NULL)
		exit(EXIT_FAILURE);
#endif

	return result;
}

int main(int argc, char *argv[]) {
	char part[512],
	     pathbuf[512];
	unsigned int i;

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
		die("Could not create socket\n");

	while (1) {
		for (i = 0; i < num_run_watches; i += 2) {
			bool running = process_runs(run_watches[i+1]);
			if (use_colors)
				snprintf(part, sizeof(part), "%s%s: %s",
					(running ? color("#00FF00") : color("#FF0000")),
					run_watches[i],
					(running ? "yes" : "no"));
			else snprintf(part, sizeof(part), "%s: %s", run_watches[i], (running ? "yes" : "no"));
			snprintf(pathbuf, sizeof(pathbuf), "%s%s", order[ORDER_RUN], run_watches[i]);
			write_to_statusbar(pathbuf, part, false);
		}

		if (wlan_interface)
			write_to_statusbar(concat(order[ORDER_WLAN], "wlan"), get_wireless_info(), false);
		if (eth_interface)
			write_to_statusbar(concat(order[ORDER_ETH], "eth"), get_eth_info(), false);
		if (battery_path)
			write_to_statusbar(concat(order[ORDER_BATTERY], "battery"), get_battery_info(), false);

		/* Get load */
#ifdef LINUX
		int load_avg;
		if ((load_avg = open("/proc/loadavg", O_RDONLY)) == -1)
			die("Could not open /proc/loadavg\n");
		(void)read(load_avg, part, sizeof(part));
		(void)close(load_avg);
		*skip_character(part, ' ', 3) = '\0';
#else
		/* TODO: correctly check for NetBSD, check if it works the same on *BSD */
		struct loadavg load;
		size_t length = sizeof(struct loadavg);
		int mib[2] = { CTL_VM, VM_LOADAVG };
		if (sysctl(mib, 2, &load, &length, NULL, 0) < 0)
			die("Could not sysctl({ CTL_VM, VM_LOADAVG })\n");
		double scale = load.fscale;
		(void)snprintf(part, sizeof(part), "%.02f %.02f %.02f",
				(double)load.ldavg[0] / scale,
				(double)load.ldavg[1] / scale,
				(double)load.ldavg[2] / scale);
#endif
		write_to_statusbar(concat(order[ORDER_LOAD], "load"), part, !time_format);

		if (time_format) {
			/* Get date & time */
			time_t current_time = time(NULL);
			struct tm *current_tm = localtime(&current_time);
			(void)strftime(part, sizeof(part), time_format, current_tm);
			write_to_statusbar(concat(order[ORDER_TIME], "time"), part, true);
		}

		sleep(interval);
	}
}
