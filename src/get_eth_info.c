// vim:ts=8:expandtab
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "i3status.h"

#if defined(LINUX)
#include <linux/ethtool.h>
#include <linux/sockios.h>
#define PART_ETHSPEED  "E: %s (%d Mbit/s)"
#endif

#if defined(__FreeBSD__)
#include <net/if_media.h>
#define IFM_TYPE_MATCH(dt, t)                       \
        (IFM_TYPE((dt)) == 0 || IFM_TYPE((dt)) == IFM_TYPE((t)))

#define PART_ETHSPEED  "E: %s (%s)"

#endif

/*
 * Combines ethernet IP addresses and speed (if requested) for displaying
 *
 */
const char *get_eth_info() {
        static char part[512];
#if defined(LINUX)
        int ethspeed=0;
#elif defined(__FreeBSD__)
        char *ethspeed;
#endif
        const char *ip_address = get_ip_addr(eth_interface);

        if (ip_address == NULL) {
                (void)snprintf(part, sizeof(part), "E: down");
                return part;
        }

        if (get_ethspeed) {
#if defined(LINUX)
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
#elif defined(__FreeBSD__)
                struct ifmediareq ifm;
                (void)memset(&ifm, 0, sizeof(ifm));
                (void)strncpy(ifm.ifm_name, eth_interface, sizeof(ifm.ifm_name));
                int ret = ioctl(general_socket, SIOCGIFMEDIA, (caddr_t)&ifm);

                /* Get the description of the media type, partially taken from
                 * FreeBSD's ifconfig */
                const struct ifmedia_description *desc;
                struct ifmedia_description ifm_subtype_descriptions[] =
                        IFM_SUBTYPE_ETHERNET_DESCRIPTIONS;

                for (desc = ifm_subtype_descriptions;
                     desc->ifmt_string != NULL;
                     desc++) {
                    if (IFM_TYPE_MATCH(desc->ifmt_word, ifm.ifm_active) &&
                        IFM_SUBTYPE(desc->ifmt_word) == IFM_SUBTYPE(ifm.ifm_active))
                        break;
                }
                ethspeed = (desc->ifmt_string != NULL ? desc->ifmt_string : "?");
#endif
        }

        if (get_ethspeed)
                (void)snprintf(part, sizeof(part), PART_ETHSPEED, ip_address, ethspeed);
        else (void)snprintf(part, sizeof(part), "E: %s", ip_address);

        return part;
}
