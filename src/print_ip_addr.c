// vim:ts=8:expandtab
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
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
const char *get_ip_addr(const char *interface) {
        static char part[512];
        socklen_t len = sizeof(struct sockaddr_in);
        memset(part, 0, sizeof(part));

        struct ifaddrs *ifaddr, *addrp;

        getifaddrs(&ifaddr);

        if (ifaddr == NULL)
                return NULL;

        addrp = ifaddr;

        /* Skip until we are at the AF_INET address of interface */
        for (addrp = ifaddr;

             (addrp != NULL &&
              (strcmp(addrp->ifa_name, interface) != 0 ||
               addrp->ifa_addr == NULL ||
               addrp->ifa_addr->sa_family != AF_INET));

             addrp = addrp->ifa_next) {
                /* Check if the interface is down */
                if (strcmp(addrp->ifa_name, interface) == 0 &&
                    (addrp->ifa_flags & IFF_RUNNING) == 0) {
                        freeifaddrs(ifaddr);
                        return NULL;
                }
        }

        if (addrp == NULL) {
                freeifaddrs(ifaddr);
                return "no IP";
        }

        int ret;
        if ((ret = getnameinfo(addrp->ifa_addr, len, part, sizeof(part), NULL, 0, NI_NUMERICHOST)) != 0) {
                fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(ret));
                freeifaddrs(ifaddr);
                return "no IP";
        }

        freeifaddrs(ifaddr);
        return part;
}

