// vim:ts=4:sw=4:expandtab
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
    int interface_len = strlen(interface);

    getifaddrs(&ifaddr);

    if (ifaddr == NULL)
        return NULL;

    /* Skip until we are at the input family address of interface */
    for (addrp = ifaddr;

         (addrp != NULL &&
          (strncmp(addrp->ifa_name, interface, interface_len) != 0 ||
           addrp->ifa_addr == NULL ||
           addrp->ifa_addr->sa_family != family));

         addrp = addrp->ifa_next) {
        /* Check if the interface is down */
        if (strncmp(addrp->ifa_name, interface, interface_len) != 0)
            continue;
        found = true;
        if ((addrp->ifa_flags & IFF_RUNNING) == 0) {
            freeifaddrs(ifaddr);
            return NULL;
        }
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
