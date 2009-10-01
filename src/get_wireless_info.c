// vim:ts=8:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <iwlib.h>

#include "i3status.h"

const char *get_wireless_essid() {
        static char part[512];
#ifdef LINUX
        int skfd;
        if ((skfd = iw_sockets_open()) < 0) {
                perror("socket");
                exit(-1);
        }
        struct wireless_config cfg;
        if (iw_get_basic_config(skfd, wlan_interface, &cfg) >= 0)
                snprintf(part, sizeof(part), "%s", cfg.essid);
        else part[0] = '\0';
        (void)close(skfd);
#else
        part[0] = '\0';
#endif
        return part;
}

/*
 * Just parses /proc/net/wireless looking for lines beginning with
 * wlan_interface, extracting the quality of the link and adding the
 * current IP address of wlan_interface.
 *
 */
const char *get_wireless_info() {
        char buf[1024];
        static char part[512];
        char *interfaces;
        memset(buf, 0, sizeof(buf));
        memset(part, 0, sizeof(part));

        if (!slurp("/proc/net/wireless", buf, sizeof(buf)))
                die("Could not open \"/proc/net/wireless\"\n");

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
                        (void)snprintf(part, sizeof(part), "%sW: down%s", color("#FF0000"), endcolor());
                } else (void)snprintf(part, sizeof(part), "%sW: (%03d%% at %s) %s%s",
                                color("#00FF00"), quality, get_wireless_essid(), get_ip_addr(wlan_interface), endcolor());
                return part;
        }

        return part;
}

