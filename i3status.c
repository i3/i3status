/*
 * vim:ts=8:expandtab
 *
 * i3status – Generates a status line for dzen2 or xmobar
 *
 *
 * Copyright © 2008-2009 Michael Stapelberg and contributors
 * Copyright © 2009 Thorsten Toepper <atsutane at freethoughts dot de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Michael Stapelberg nor the names of contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <getopt.h>
#include <signal.h>
#include <confuse.h>

#include "i3status.h"

/* socket file descriptor for general purposes */
int general_socket;

cfg_t *cfg, *cfg_general;

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
        unsigned int j;

        cfg_opt_t general_opts[] = {
                CFG_BOOL("colors", 1, CFGF_NONE),
                CFG_INT("interval", 1, CFGF_NONE),
                CFG_END()
        };

        cfg_opt_t run_watch_opts[] = {
                CFG_STR("pidfile", NULL, CFGF_NONE),
                CFG_STR("format", "%title: %status", CFGF_NONE),
                CFG_END()
        };

        cfg_opt_t wireless_opts[] = {
                CFG_STR("format_up", "W: (%quality at %essid) %ip", CFGF_NONE),
                CFG_STR("format_down", "W: down", CFGF_NONE),
                CFG_END()
        };

        cfg_opt_t ethernet_opts[] = {
                CFG_STR("format", "E: %ip (%speed)", CFGF_NONE),
                CFG_END()
        };

        cfg_opt_t ipv6_opts[] = {
                CFG_STR("format", "%ip", CFGF_NONE),
                CFG_END()
        };

        cfg_opt_t battery_opts[] = {
                CFG_STR("format", "%status %remaining", CFGF_NONE),
                CFG_END()
        };

        cfg_opt_t time_opts[] = {
                CFG_STR("format", "%d.%m.%Y %H:%M:%S", CFGF_NONE),
                CFG_END()
        };

        cfg_opt_t load_opts[] = {
                CFG_STR("format", "%5min %10min %15min", CFGF_NONE),
                CFG_END()
        };

        cfg_opt_t temp_opts[] = {
                CFG_STR("format", "%degrees C", CFGF_NONE),
                CFG_END()
        };

        cfg_opt_t disk_opts[] = {
                CFG_STR("format", "%free", CFGF_NONE),
                CFG_END()
        };

        cfg_opt_t opts[] = {
                CFG_STR_LIST("order", "{ipv6,\"run_watch DHCP\",\"wireless wlan0\",\"ethernet eth0\",\"battery 0\",\"cpu_temperature 0\",load,time}", CFGF_NONE),
                CFG_SEC("general", general_opts, CFGF_NONE),
                CFG_SEC("run_watch", run_watch_opts, CFGF_TITLE | CFGF_MULTI),
                CFG_SEC("wireless", wireless_opts, CFGF_TITLE | CFGF_MULTI),
                CFG_SEC("ethernet", ethernet_opts, CFGF_TITLE | CFGF_MULTI),
                CFG_SEC("battery", battery_opts, CFGF_TITLE | CFGF_MULTI),
                CFG_SEC("cpu_temperature", temp_opts, CFGF_TITLE | CFGF_MULTI),
                CFG_SEC("disk", disk_opts, CFGF_TITLE | CFGF_MULTI),
                CFG_SEC("ipv6", ipv6_opts, CFGF_TITLE),
                CFG_SEC("time", time_opts, CFGF_NONE),
                CFG_SEC("load", load_opts, CFGF_NONE),
                CFG_END()
        };

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

        while ((o = getopt_long(argc, argv, "c:h", long_options, &option_index)) != -1)
                if ((char)o == 'c')
                        configfile = optarg;
                else if ((char)o == 'h') {
                        printf("i3status © 2008-2009 Michael Stapelberg\n"
                                "Syntax: %s [-c <configfile>]\n", argv[0]);
                        return 0;
                }

        cfg = cfg_init(opts, CFGF_NONE);
        if (cfg_parse(cfg, configfile) == CFG_PARSE_ERROR)
                return EXIT_FAILURE;

        cfg_general = cfg_getsec(cfg, "general");

        if ((general_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
                die("Could not create socket\n");

        while (1) {
                for (j = 0; j < cfg_size(cfg, "order"); j++) {
                        if (j > 0)
                                print_seperator();

                        const char *current = cfg_getnstr(cfg, "order", j);

                        CASE_SEC("ipv6")
                                print_ipv6_info(cfg_getstr(sec, "format"));

                        CASE_SEC_TITLE("wireless")
                                print_wireless_info(title, cfg_getstr(sec, "format_up"), cfg_getstr(sec, "format_down"));

                        CASE_SEC_TITLE("ethernet")
                                print_eth_info(title, cfg_getstr(sec, "format"));

                        CASE_SEC_TITLE("battery")
                                print_battery_info(atoi(title), cfg_getstr(sec, "format"));

                        CASE_SEC_TITLE("run_watch")
                                print_run_watch(title, cfg_getstr(sec, "pidfile"), cfg_getstr(sec, "format"));

                        CASE_SEC_TITLE("disk")
                                print_disk_info(title, cfg_getstr(sec, "format"));

                        CASE_SEC("load")
                                print_load(cfg_getstr(sec, "format"));

                        CASE_SEC("time")
                                print_time(cfg_getstr(sec, "format"));

                        CASE_SEC_TITLE("cpu_temperature")
                                print_cpu_temperature_info(atoi(title), cfg_getstr(sec, "format"));
                }
                printf("\n");
                fflush(stdout);

                sleep(cfg_getint(cfg_general, "interval"));
        }
}
