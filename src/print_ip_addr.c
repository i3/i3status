// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>

#include "i3status.h"

/*
 * Return a copy of the .ifa_name field passed as argument where the optional
 * IP label, if present, is removed.
 *
 * example:
 * - strip_optional_label("eth0") => "eth0"
 * - strip_optional_label("eth0:label") => "eth0"
 *
 * The memory for the returned string is obtained with malloc(3), and can be
 * freed with free(3).
 *
 *
 */
static char *strip_optional_label(const char *ifa_name) {
    char *copy = sstrdup(ifa_name);

    char *ptr = strchr(copy, ':');

    if (ptr) {
        *ptr = '\0';
    }

    return copy;
}

/*
 * Return the IP address for the given interface or "no IP" if the
 * interface is up and running but hasn't got an IP address yet
 *
 */
const char *get_ip_addr(const char *interface, int family) {
    static char part[512];
    socklen_t len = 0;
    if (family == AF_INET)
        len = sizeof(struct sockaddr_in);
    else if (family == AF_INET6)
        len = sizeof(struct sockaddr_in6);

    memset(part, 0, sizeof(part));

    struct ifaddrs *ifaddr, *addrp;
    bool found = false;

    getifaddrs(&ifaddr);

    if (ifaddr == NULL)
        return NULL;

    /* Skip until we are at the input family address of interface */
    for (addrp = ifaddr; addrp != NULL; addrp = addrp->ifa_next) {
        /* Strip the label if present in the .ifa_name field. */
        char *stripped_ifa_name = strip_optional_label(addrp->ifa_name);

        bool name_matches = strcmp(stripped_ifa_name, interface) != 0;
        free(stripped_ifa_name);
        if (name_matches) {
            /* The interface does not have the right name, skip it. */
            continue;
        }

        if (addrp->ifa_addr != NULL && addrp->ifa_addr->sa_family == family) {
            /* We found the right interface with the right address. */
            break;
        }

        /* Check if the interface is down. If it is, no need to look any
         * further. */
        if ((addrp->ifa_flags & IFF_RUNNING) == 0) {
            freeifaddrs(ifaddr);
            return NULL;
        }

        found = true;
    }

    if (addrp == NULL) {
        freeifaddrs(ifaddr);
        return (found ? "no IP" : NULL);
    }

    int ret;
    if ((ret = getnameinfo(addrp->ifa_addr, len, part, sizeof(part), NULL, 0, NI_NUMERICHOST)) != 0) {
        fprintf(stderr, "i3status: getnameinfo(): %s\n", gai_strerror(ret));
        freeifaddrs(ifaddr);
        return "no IP";
    }

    freeifaddrs(ifaddr);
    return part;
}
