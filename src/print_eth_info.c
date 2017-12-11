// vim:ts=4:sw=4:expandtab
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#if defined(LINUX)
#include <linux/ethtool.h>
#include <linux/sockios.h>
#define PART_ETHSPEED "E: %s (%d Mbit/s)"
#endif

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#include <net/if_media.h>

#define PART_ETHSPEED "E: %s (%s)"
#endif

#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <errno.h>
#include <net/if_media.h>
#endif

static int print_eth_speed(char *outwalk, const char *interface) {
#if defined(LINUX)
    /* This code path requires root privileges */
    int ethspeed = 0;
    struct ifreq ifr;
    struct ethtool_cmd ecmd;

    ecmd.cmd = ETHTOOL_GSET;
    (void)memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_data = (caddr_t)&ecmd;
    (void)strcpy(ifr.ifr_name, interface);
    if (ioctl(general_socket, SIOCETHTOOL, &ifr) == 0) {
        ethspeed = (ecmd.speed == USHRT_MAX ? 0 : ecmd.speed);
        return sprintf(outwalk, "%d Mbit/s", ethspeed);
    } else
        return sprintf(outwalk, "?");
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
    const char *ethspeed;
    struct ifmediareq ifm;
    (void)memset(&ifm, 0, sizeof(ifm));
    (void)strncpy(ifm.ifm_name, interface, sizeof(ifm.ifm_name));
    int ret;
#ifdef SIOCGIFXMEDIA
    ret = ioctl(general_socket, SIOCGIFXMEDIA, (caddr_t)&ifm);
    if (ret < 0)
#endif
        ret = ioctl(general_socket, SIOCGIFMEDIA, (caddr_t)&ifm);
    if (ret < 0)
        return sprintf(outwalk, "?");

    /* Get the description of the media type, partially taken from
     * FreeBSD's ifconfig */
    const struct ifmedia_description *desc;
    static struct ifmedia_description ifm_subtype_descriptions[] =
        IFM_SUBTYPE_ETHERNET_DESCRIPTIONS;

    if (IFM_TYPE(ifm.ifm_active) != IFM_ETHER)
        return sprintf(outwalk, "?");
    if (ifm.ifm_status & IFM_AVALID && !(ifm.ifm_status & IFM_ACTIVE))
        return sprintf(outwalk, "no carrier");
    for (desc = ifm_subtype_descriptions;
         desc->ifmt_string != NULL;
         desc++) {
        if (desc->ifmt_word == IFM_SUBTYPE(ifm.ifm_active))
            break;
    }
    ethspeed = (desc->ifmt_string != NULL ? desc->ifmt_string : "?");
    return sprintf(outwalk, "%s", ethspeed);
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    const char *ethspeed;
    struct ifmediareq ifmr;

    (void)memset(&ifmr, 0, sizeof(ifmr));
    (void)strlcpy(ifmr.ifm_name, interface, sizeof(ifmr.ifm_name));

    if (ioctl(general_socket, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
        if (errno != E2BIG)
            return sprintf(outwalk, "?");
    }

    struct ifmedia_description *desc;
    struct ifmedia_description ifm_subtype_descriptions[] =
        IFM_SUBTYPE_DESCRIPTIONS;

    for (desc = ifm_subtype_descriptions; desc->ifmt_string != NULL; desc++) {
        /*
		 * Skip these non-informative values and go right ahead to the
		 * actual speeds.
		 */
        if (BEGINS_WITH(desc->ifmt_string, "autoselect") ||
            BEGINS_WITH(desc->ifmt_string, "auto"))
            continue;

        if (IFM_TYPE_MATCH(desc->ifmt_word, ifmr.ifm_active) &&
            IFM_SUBTYPE(desc->ifmt_word) == IFM_SUBTYPE(ifmr.ifm_active))
            break;
    }
    ethspeed = (desc->ifmt_string != NULL ? desc->ifmt_string : "?");
    return sprintf(outwalk, "%s", ethspeed);

#else
    return sprintf(outwalk, "?");
#endif
}

/*
 * Combines ethernet IP addresses and speed (if requested) for displaying
 *
 * Table summarizing what is the decision to prefer IPv4 or IPv6
 * based their values.
 *
 * | ipv4_address | ipv6_address | Chosen IP | Color             |
 * |--------------|--------------|-----------|-------------------|
 * | NULL         | NULL         | None      | bad (red)         |
 * | NULL         | no IP        | IPv6      | degraded (orange) |
 * | NULL         | ::1/128      | IPv6      | ok (green)        |
 * | no IP        | NULL         | IPv4      | degraded          |
 * | no IP        | no IP        | IPv4      | degraded          |
 * | no IP        | ::1/128      | IPv6      | ok                |
 * | 127.0.0.1    | NULL         | IPv4      | ok                |
 * | 127.0.0.1    | no IP        | IPv4      | ok                |
 * | 127.0.0.1    | ::1/128      | IPv4      | ok                |
 */
void print_eth_info(yajl_gen json_gen, char *buffer, const char *interface, const char *format_up, const char *format_down) {
    const char *walk;
    char *outwalk = buffer;

    INSTANCE(interface);

    char *ipv4_address = sstrdup(get_ip_addr(interface, AF_INET));
    char *ipv6_address = sstrdup(get_ip_addr(interface, AF_INET6));

    /*
     * Removing '%' and following characters from IPv6 since the interface identifier is redundant,
     * as the output already includes the interface name.
    */
    if (ipv6_address != NULL) {
        char *prct_ptr = strstr(ipv6_address, "%");
        if (prct_ptr != NULL) {
            *prct_ptr = '\0';
        }
    }

    bool prefer_ipv4 = true;
    if (ipv4_address == NULL) {
        if (ipv6_address == NULL) {
            START_COLOR("color_bad");
            outwalk += sprintf(outwalk, "%s", format_down);
            goto out;
        } else {
            prefer_ipv4 = false;
        }
    } else if (BEGINS_WITH(ipv4_address, "no IP") && ipv6_address != NULL && !BEGINS_WITH(ipv6_address, "no IP")) {
        prefer_ipv4 = false;
    }

    const char *ip_address = (prefer_ipv4) ? ipv4_address : ipv6_address;
    if (BEGINS_WITH(ip_address, "no IP")) {
        START_COLOR("color_degraded");
    } else {
        START_COLOR("color_good");
    }
    for (walk = format_up; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "ip")) {
            outwalk += sprintf(outwalk, "%s", ip_address);
            walk += strlen("ip");
        } else if (BEGINS_WITH(walk + 1, "speed")) {
            outwalk += print_eth_speed(outwalk, interface);
            walk += strlen("speed");
        }
    }

out:
    END_COLOR;
    free(ipv4_address);
    free(ipv6_address);
    OUTPUT_FULL_TEXT(buffer);
}
