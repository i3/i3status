// vim:ts=8:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#ifdef LINUX
#include <iwlib.h>
#endif

#include "i3status.h"

static const char *get_wireless_essid(const char *interface) {
        static char part[512];
#ifdef LINUX
        int skfd;
        if ((skfd = iw_sockets_open()) < 0) {
                perror("socket");
                exit(-1);
        }
        struct wireless_config wcfg;
        if (iw_get_basic_config(skfd, interface, &wcfg) >= 0)
                snprintf(part, sizeof(part), "%s", wcfg.essid);
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
void print_wireless_info(const char *interface, const char *format_up, const char *format_down) {
        char buf[1024];
        int quality = -1;
        char *interfaces;
        const char *walk;
        memset(buf, 0, sizeof(buf));

        if (!slurp("/proc/net/wireless", buf, sizeof(buf)))
                die("Could not open \"/proc/net/wireless\"\n");

        interfaces = skip_character(buf, '\n', 1) + 1;
        while ((interfaces = skip_character(interfaces, '\n', 1)+1) < buf+strlen(buf)) {
                while (isspace((int)*interfaces))
                        interfaces++;
                if (!BEGINS_WITH(interfaces, interface))
                        continue;
                if (sscanf(interfaces, "%*[^:]: 0000 %d", &quality) != 1)
                        continue;
                break;
        }

        /* Interface could not be found */
        if (quality == -1)
                return;

        if ((quality == UCHAR_MAX) || (quality == 0)) {
                walk = format_down;
                printf("%s", color("#FF0000"));
        } else {
                printf("%s", color("#00FF00"));
                walk = format_up;
        }

        for (; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        putchar(*walk);
                        continue;
                }

                if (BEGINS_WITH(walk+1, "quality")) {
                        (void)printf("%03d%%", quality);
                        walk += strlen("quality");
                }

                if (BEGINS_WITH(walk+1, "essid")) {
                        (void)printf("%s", get_wireless_essid(interface));
                        walk += strlen("essid");
                }

                if (BEGINS_WITH(walk+1, "ip")) {
                        (void)printf("%s", get_ip_addr(interface));
                        walk += strlen("ip");
                }
        }

        (void)printf("%s", endcolor());
}
