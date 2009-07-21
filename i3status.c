/*
 * vim:ts=8:expandtab
 *
 * i3status – Generates a status line for dzen2 or wmii
 *
 *
 * Copyright © 2008-2009 Michael Stapelberg and contributors
 * Copyright © 2009 Thorsten Toepper <atsutane at freethoughts dot de>
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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <getopt.h>
#include <signal.h>

#include "queue.h"

#include "i3status.h"

struct battery_head batteries;

/* socket file descriptor for general purposes */
int general_socket;

const char *wlan_interface;
const char *eth_interface;
const char *wmii_path;
const char *time_format;
bool use_colors;
bool get_ethspeed;
bool get_cpu_temperature;
char *thermal_zone;
const char *wmii_normcolors = "#222222 #333333";
char order[MAX_ORDER][2];
const char **run_watches;
unsigned int num_run_watches;
unsigned int interval = 1;

/*
 * Exit upon SIGPIPE because when we have nowhere to write to, gathering
 * system information is pointless.
 *
 */
void sigpipe(int signum) {
        fprintf(stderr, "Received SIGPIPE, exiting\n");
        exit(1);
}

int main(int argc, char *argv[]) {
        char part[512],
             pathbuf[512];
        unsigned int i;

        char *configfile = PREFIX "/etc/i3status.conf";
        int o, option_index = 0;
        struct option long_options[] = {
                {"config", required_argument, 0, 'c'},
                {"help", no_argument, 0, 'h'},
                {0, 0, 0, 0}
        };

        struct sigaction action;
        memset(&action, 0, sizeof(struct sigaction));
        action.sa_handler = sigpipe;
        sigaction(SIGPIPE, &action, NULL);

        SIMPLEQ_INIT(&batteries);

        while ((o = getopt_long(argc, argv, "c:h", long_options, &option_index)) != -1)
                if ((char)o == 'c')
                        configfile = optarg;
                else if ((char)o == 'h') {
                        printf("i3status (c) 2008-2009 Michael Stapelberg\n"
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
                struct battery *current_battery;
                SIMPLEQ_FOREACH(current_battery, &batteries, batteries) {
                        write_to_statusbar(concat(order[ORDER_BATTERY], "battery"), get_battery_info(current_battery), false);
                }
                if (get_cpu_temperature)
                        write_to_statusbar(concat(order[ORDER_CPU_TEMPERATURE], "cpu_temperature"), get_cpu_temperature_info(), false);

                write_to_statusbar(concat(order[ORDER_LOAD], "load"), get_load(), !time_format);

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
