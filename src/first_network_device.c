// vim:ts=4:sw=4:expandtab
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ifaddrs.h>

#include "i3status.h"

static bool sysfs_devtype(char *dest, size_t n, const char *ifnam) {
    FILE *fp;
    char buf[1024];

    snprintf(buf, sizeof(buf), "/sys/class/net/%s/uevent", ifnam);
    if ((fp = fopen(buf, "r")) == NULL)
        return false;

    dest[0] = '\0';

    while (fgets(buf, sizeof(buf), fp)) {
        size_t slen;
        char *s;

        slen = strlen(buf);
        /* Line is too long to fit in the buffer */
        if (buf[slen - 1] != '\n' && !feof(fp))
            break;
        if ((s = strchr(buf, '='))) {
            if (strncmp(buf, "DEVTYPE", s - buf))
                continue;
            buf[slen - 1] = '\0';
            strncpy(dest, ++s, n);
            break;
        }
    }
    fclose(fp);
    return true;
}

static net_type_t iface_type(const char *ifname) {
    char devtype[32];

    if (!sysfs_devtype(devtype, sizeof(devtype), ifname))
        return NET_TYPE_OTHER;

    if (!devtype[0])
        return NET_TYPE_ETHERNET;

    if (strcmp(devtype, "wlan") == 0)
        return NET_TYPE_WIRELESS;

    if (strcmp(devtype, "wwan") == 0)
        return NET_TYPE_OTHER;

    return NET_TYPE_OTHER;
}

const char *first_eth_interface(const net_type_t type) {
    static char *interface = NULL;
    struct ifaddrs *ifaddr, *addrp;
    net_type_t iftype;

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
        if (addrp->ifa_addr == NULL)
            continue;
        // Skip PF_PACKET addresses (MAC addresses), as they are present on any
        // ethernet interface.
        if (addrp->ifa_addr->sa_family != AF_INET &&
            addrp->ifa_addr->sa_family != AF_INET6)
            continue;
        // Skip this interface if it is a wireless interface.
        iftype = iface_type(addrp->ifa_name);
        if (iftype != type)
            continue;
        interface = strdup(addrp->ifa_name);
        break;
    }

    freeifaddrs(ifaddr);
    return interface;
}
