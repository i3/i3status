// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ifaddrs.h>
#if defined(__OpenBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>
#include <net/if.h>
#endif
#if defined(__OpenBSD__)
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#elif defined(__DragonFly__)
#include <netproto/802_11/ieee80211_ioctl.h>
#endif

#include "i3status.h"

#ifdef __OpenBSD__
#define LOOPBACK_DEV "lo0"
#else
#define LOOPBACK_DEV "lo"
#endif

#if defined(__linux__)
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
#endif

static bool is_virtual(const char *ifname) {
    char path[1024];
    char *target = NULL;
    bool is_virtual = false;

    snprintf(path, sizeof(path), "/sys/class/net/%s", ifname);
    if ((target = realpath(path, NULL))) {
        if (BEGINS_WITH(target, "/sys/devices/virtual/")) {
            is_virtual = true;
        }
    }

    free(target);
    return is_virtual;
}

static net_type_t iface_type(const char *ifname) {
#if defined(__OpenBSD__)
    /*
     *First determine if the device is a wireless device by trying two ioctl(2)
     * commands against it. If either succeeds we can be sure it's a wireless
     * device.
     * Otherwise we try another ioctl(2) to determine the interface media. If that
     * fails it's not an ethernet device eiter.
     */
    struct ifreq ifr;
    struct ieee80211_bssid bssid;
    struct ieee80211_nwid nwid;
#elif defined(__DragonFly__)
    struct ieee80211req ifr;
#endif
#if defined(__OpenBSD__) || defined(__DragonFly__)
    struct ifmediareq ifmr;
    int s;
#endif
#if defined(__OpenBSD__)
    int ibssid, inwid;
#endif
#if defined(__OpenBSD__) || defined(__DragonFly__)
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        return NET_TYPE_OTHER;

    memset(&ifr, 0, sizeof(ifr));
#endif
#if defined(__OpenBSD__)
    ifr.ifr_data = (caddr_t)&nwid;
    (void)strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    inwid = ioctl(s, SIOCG80211NWID, (caddr_t)&ifr);

    memset(&bssid, 0, sizeof(bssid));
    strlcpy(bssid.i_name, ifname, sizeof(bssid.i_name));
    ibssid = ioctl(s, SIOCG80211BSSID, &bssid);

    if (ibssid == 0 || inwid == 0) {
        close(s);
        return NET_TYPE_WIRELESS;
    }
#elif defined(__DragonFly__)
    (void)strlcpy(ifr.i_name, ifname, sizeof(ifr.i_name));
    ifr.i_type = IEEE80211_IOC_NUMSSIDS;
    if (ioctl(s, SIOCG80211, &ifr) == 0) {
        close(s);
        return NET_TYPE_WIRELESS;
    }
#endif
#if defined(__OpenBSD__) || defined(__DragonFly__)
    (void)memset(&ifmr, 0, sizeof(ifmr));
    (void)strlcpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));

    if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
        close(s);
        return NET_TYPE_OTHER;
    } else {
        close(s);
        return NET_TYPE_ETHERNET;
    }
#elif defined(__linux__)
    char devtype[32];

    if (!sysfs_devtype(devtype, sizeof(devtype), ifname))
        return NET_TYPE_OTHER;

    /* Default to Ethernet when no devtype is available */
    if (!devtype[0])
        return NET_TYPE_ETHERNET;

    if (strcmp(devtype, "wlan") == 0)
        return NET_TYPE_WIRELESS;

    if (strcmp(devtype, "wwan") == 0)
        return NET_TYPE_OTHER;

    return NET_TYPE_OTHER;
#endif
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
        if (strncasecmp(LOOPBACK_DEV, addrp->ifa_name, strlen(LOOPBACK_DEV)) == 0)
            continue;
        if (addrp->ifa_addr == NULL)
            continue;
#ifdef __OpenBSD__
        if (addrp->ifa_addr->sa_family != AF_LINK)
            continue;
#else
        // Skip PF_PACKET addresses (MAC addresses), as they are present on any
        // ethernet interface.
        if (addrp->ifa_addr->sa_family != AF_INET &&
            addrp->ifa_addr->sa_family != AF_INET6)
            continue;
#endif /* !__OpenBSD__ */
        // Skip this interface if it is a wireless interface.
        iftype = iface_type(addrp->ifa_name);
        if (iftype != type)
            continue;
        if (is_virtual(addrp->ifa_name))
            continue;
        interface = strdup(addrp->ifa_name);
        break;
    }

    freeifaddrs(ifaddr);
    return interface;
}
