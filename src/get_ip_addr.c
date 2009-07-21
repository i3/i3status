#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include "i3status.h"

/*
 * Return the IP address for the given interface or "no IP" if the
 * interface is up and running but hasn't got an IP address yet
 *
 */
const char *get_ip_addr(const char *interface) {
        static char part[512];
        struct ifreq ifr;
        socklen_t len = sizeof(struct sockaddr_in);
        memset(part, 0, sizeof(part));

        /* First check if the interface is running */
        (void)strcpy(ifr.ifr_name, interface);
        if (ioctl(general_socket, SIOCGIFFLAGS, &ifr) < 0 ||
            !(ifr.ifr_flags & IFF_RUNNING))
                return NULL;

        /* Interface is up, get the IP address */
        (void)strcpy(ifr.ifr_name, interface);
        ifr.ifr_addr.sa_family = AF_INET;
        if (ioctl(general_socket, SIOCGIFADDR, &ifr) < 0)
                return "no IP";

	int ret;
	if ((ret = getnameinfo(&ifr.ifr_addr, len, part, sizeof(part), NULL, 0, NI_NUMERICHOST)) != 0) {
		fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(ret));
		return "no IP";
	}

        return part;
}

