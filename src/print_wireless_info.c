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

#ifdef LINUX
static int skfd = -1;

static int open_skfd() {
    if (skfd == -1) {
        skfd = iw_sockets_open();
        if (skfd < 0) {
            perror("iw_sockets_open");
            return 0;
        }
    }
    return -1;
}

static void close_skfd() {
    if (skfd != -1) {
        close(skfd);
        skfd = -1;
    }
}
#endif

const char *get_wireless_essid(const char *interface) {
    static char part[512];
    part[0] = '\0';
#ifdef LINUX
    if (open_skfd()) {
        wireless_config wcfg;
        if (iw_get_basic_config(skfd, interface, &wcfg) >= 0)
            snprintf(part, sizeof(part), "%s", wcfg.essid);
    }
#endif
    return part;
}

int get_wireless_quality_max(const char *interface) {
#ifdef LINUX
    if (open_skfd()) {
        iwrange range;
        if (iw_get_range_info(skfd, interface, &range) >= 0)
            return range.max_qual.qual;
    }
#endif
    return 0;
}

/*
 * Just parses /proc/net/wireless looking for lines beginning with
 * wlan_interface, extracting the quality of the link and adding the
 * current IP address of wlan_interface.
 *
 */
void print_wireless_info(const char *interface, const char *format_up, const char *format_down) {
        char buf[1024];
        int quality = 0;
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
                        int max_qual = get_wireless_quality_max(interface);
                        if (max_qual && max_qual >= quality)
                                printf("%d%%", (int)(quality * (100.0 / max_qual)));
                        else
                                printf("%d", quality);
                        walk += strlen("quality");
                }

                if (BEGINS_WITH(walk+1, "essid")) {
                        (void)printf("%s", get_wireless_essid(interface));
                        walk += strlen("essid");
                }

                if (BEGINS_WITH(walk+1, "ip")) {
                        const char *ip_address = get_ip_addr(interface);
                        if (ip_address != NULL)
                                (void)printf("%s", get_ip_addr(interface));
                        else (void)printf("no IP");
                        walk += strlen("ip");
                }
        }

#ifdef LINUX
        close_skfd();
#endif

        (void)printf("%s", endcolor());
}
