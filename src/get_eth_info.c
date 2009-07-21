#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <net/if.h>

#ifdef LINUX
#include <linux/ethtool.h>
#include <linux/sockios.h>
#endif

#include "i3status.h"

/*
 * Combines ethernet IP addresses and speed (if requested) for displaying
 *
 */
const char *get_eth_info() {
        static char part[512];
        const char *ip_address = get_ip_addr(eth_interface);
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
