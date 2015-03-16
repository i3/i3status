// vim:ts=4:sw=4:expandtab
#include <sys/stat.h>
#include <stdlib.h>
#include <ifaddrs.h>

#include "i3status.h"

const char *first_eth_interface(const net_type_t type) {
    static char *interface = NULL;
    struct ifaddrs *ifaddr, *addrp;
    struct stat stbuf;
    static char path[1024];

    getifaddrs(&ifaddr);

    if (ifaddr == NULL)
        return NULL;

    free(interface);
    interface = NULL;
    for (addrp = ifaddr;
         addrp != NULL;
         addrp = addrp->ifa_next) {
        if (strncasecmp("lo", addrp->ifa_name, strlen("lo")) == 0)
            continue;
        // Skip this interface if it is a wireless interface.
        snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", addrp->ifa_name);
        const bool is_wireless = (stat(path, &stbuf) == 0);
        if ((is_wireless && type == NET_TYPE_ETHERNET) ||
            (!is_wireless && type == NET_TYPE_WIRELESS))
            continue;
        interface = strdup(addrp->ifa_name);
        break;
    }

    freeifaddrs(ifaddr);
    return interface;
}
