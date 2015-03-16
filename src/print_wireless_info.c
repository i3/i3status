// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#ifdef LINUX
#include <iwlib.h>
#else
#ifndef __FreeBSD__
#define IW_ESSID_MAX_SIZE 32
#endif
#endif

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <unistd.h>
#define IW_ESSID_MAX_SIZE IEEE80211_NWID_LEN
#endif

#ifdef __DragonFly__
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_media.h>
#include <netproto/802_11/ieee80211.h>
#include <netproto/802_11/ieee80211_ioctl.h>
#include <unistd.h>
#define IW_ESSID_MAX_SIZE IEEE80211_NWID_LEN
#endif

#ifdef __OpenBSD__
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#endif

#include "i3status.h"

#define WIRELESS_INFO_FLAG_HAS_ESSID (1 << 0)
#define WIRELESS_INFO_FLAG_HAS_QUALITY (1 << 1)
#define WIRELESS_INFO_FLAG_HAS_SIGNAL (1 << 2)
#define WIRELESS_INFO_FLAG_HAS_NOISE (1 << 3)
#define WIRELESS_INFO_FLAG_HAS_FREQUENCY (1 << 4)

#define PERCENT_VALUE(value, total) ((int)(value * 100 / (float)total + 0.5f))

typedef struct {
    int flags;
    char essid[IW_ESSID_MAX_SIZE + 1];
    int quality;
    int quality_max;
    int quality_average;
    int signal_level;
    int signal_level_max;
    int noise_level;
    int noise_level_max;
    int bitrate;
    double frequency;
} wireless_info_t;

static int get_wireless_info(const char *interface, wireless_info_t *info) {
    memset(info, 0, sizeof(wireless_info_t));

#ifdef LINUX
    int skfd = iw_sockets_open();
    if (skfd < 0) {
        perror("iw_sockets_open");
        return 0;
    }

    wireless_config wcfg;
    if (iw_get_basic_config(skfd, interface, &wcfg) < 0) {
        close(skfd);
        return 0;
    }

    if (wcfg.has_essid && wcfg.essid_on) {
        info->flags |= WIRELESS_INFO_FLAG_HAS_ESSID;
        strncpy(&info->essid[0], wcfg.essid, IW_ESSID_MAX_SIZE);
        info->essid[IW_ESSID_MAX_SIZE] = '\0';
    }

    if (wcfg.has_freq) {
        info->frequency = wcfg.freq;
        info->flags |= WIRELESS_INFO_FLAG_HAS_FREQUENCY;
    }

    /* If the function iw_get_stats does not return proper stats, the
     * wifi is considered as down.
     * Since ad-hoc network does not have theses stats, we need to return
     * here for this mode. */
    if (wcfg.mode == 1) {
        close(skfd);
        return 1;
    }

    /* Wireless quality is a relative value in a driver-specific range.
     * Signal and noise level can be either relative or absolute values
     * in dBm. Furthermore, noise and quality can be expressed directly
     * in dBm or in RCPI (802.11k), which we convert to dBm. When those
     * values are expressed directly in dBm, they range from -192 to 63,
     * and since the values are packed into 8 bits, we need to perform
     * 8-bit arithmetic on them. Assume absolute values if everything
     * else fails (driver bug). */

    iwrange range;
    if (iw_get_range_info(skfd, interface, &range) < 0) {
        close(skfd);
        return 0;
    }

    iwstats stats;
    if (iw_get_stats(skfd, interface, &stats, &range, 1) < 0) {
        close(skfd);
        return 0;
    }

    if (stats.qual.level != 0 || (stats.qual.updated & (IW_QUAL_DBM | IW_QUAL_RCPI))) {
        if (!(stats.qual.updated & IW_QUAL_QUAL_INVALID)) {
            info->quality = stats.qual.qual;
            info->quality_max = range.max_qual.qual;
            info->quality_average = range.avg_qual.qual;
            info->flags |= WIRELESS_INFO_FLAG_HAS_QUALITY;
        }

        if (stats.qual.updated & IW_QUAL_RCPI) {
            if (!(stats.qual.updated & IW_QUAL_LEVEL_INVALID)) {
                info->signal_level = stats.qual.level / 2.0 - 110 + 0.5;
                info->flags |= WIRELESS_INFO_FLAG_HAS_SIGNAL;
            }
            if (!(stats.qual.updated & IW_QUAL_NOISE_INVALID)) {
                info->noise_level = stats.qual.noise / 2.0 - 110 + 0.5;
                info->flags |= WIRELESS_INFO_FLAG_HAS_NOISE;
            }
        } else {
            if ((stats.qual.updated & IW_QUAL_DBM) || stats.qual.level > range.max_qual.level) {
                if (!(stats.qual.updated & IW_QUAL_LEVEL_INVALID)) {
                    info->signal_level = stats.qual.level;
                    if (info->signal_level > 63)
                        info->signal_level -= 256;
                    info->flags |= WIRELESS_INFO_FLAG_HAS_SIGNAL;
                }
                if (!(stats.qual.updated & IW_QUAL_NOISE_INVALID)) {
                    info->noise_level = stats.qual.noise;
                    if (info->noise_level > 63)
                        info->noise_level -= 256;
                    info->flags |= WIRELESS_INFO_FLAG_HAS_NOISE;
                }
            } else {
                if (!(stats.qual.updated & IW_QUAL_LEVEL_INVALID)) {
                    info->signal_level = stats.qual.level;
                    info->signal_level_max = range.max_qual.level;
                    info->flags |= WIRELESS_INFO_FLAG_HAS_SIGNAL;
                }
                if (!(stats.qual.updated & IW_QUAL_NOISE_INVALID)) {
                    info->noise_level = stats.qual.noise;
                    info->noise_level_max = range.max_qual.noise;
                    info->flags |= WIRELESS_INFO_FLAG_HAS_NOISE;
                }
            }
        }
    } else {
        if (!(stats.qual.updated & IW_QUAL_QUAL_INVALID)) {
            info->quality = stats.qual.qual;
            info->flags |= WIRELESS_INFO_FLAG_HAS_QUALITY;
        }
        if (!(stats.qual.updated & IW_QUAL_LEVEL_INVALID)) {
            info->quality = stats.qual.level;
            info->flags |= WIRELESS_INFO_FLAG_HAS_SIGNAL;
        }
        if (!(stats.qual.updated & IW_QUAL_NOISE_INVALID)) {
            info->quality = stats.qual.noise;
            info->flags |= WIRELESS_INFO_FLAG_HAS_NOISE;
        }
    }

    struct iwreq wrq;
    if (iw_get_ext(skfd, interface, SIOCGIWRATE, &wrq) >= 0)
        info->bitrate = wrq.u.bitrate.value;

    close(skfd);
    return 1;
#endif
#if defined(__FreeBSD__) || defined(__DragonFly__)
    int s, len, inwid;
    uint8_t buf[24 * 1024], *cp;
    struct ieee80211req na;
    char network_id[IEEE80211_NWID_LEN + 1];

    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        return (0);

    memset(&na, 0, sizeof(na));
    strlcpy(na.i_name, interface, sizeof(na.i_name));
    na.i_type = IEEE80211_IOC_SSID;
    na.i_data = &info->essid[0];
    na.i_len = IEEE80211_NWID_LEN + 1;
    if ((inwid = ioctl(s, SIOCG80211, (caddr_t)&na)) == -1) {
        close(s);
        return (0);
    }
    if (inwid == 0) {
        if (na.i_len <= IEEE80211_NWID_LEN)
            len = na.i_len + 1;
        else
            len = IEEE80211_NWID_LEN + 1;
        info->essid[len - 1] = '\0';
    } else {
        close(s);
        return (0);
    }
    info->flags |= WIRELESS_INFO_FLAG_HAS_ESSID;

    memset(&na, 0, sizeof(na));
    strlcpy(na.i_name, interface, sizeof(na.i_name));
    na.i_type = IEEE80211_IOC_SCAN_RESULTS;
    na.i_data = buf;
    na.i_len = sizeof(buf);

    if (ioctl(s, SIOCG80211, (caddr_t)&na) == -1) {
        printf("fail\n");
        close(s);
        return (0);
    }

    close(s);
    len = na.i_len;
    cp = buf;
    struct ieee80211req_scan_result *sr;
    uint8_t *vp;
    sr = (struct ieee80211req_scan_result *)cp;
    vp = (u_int8_t *)(sr + 1);
    strlcpy(network_id, (const char *)vp, sr->isr_ssid_len + 1);
    if (!strcmp(network_id, &info->essid[0])) {
        info->signal_level = sr->isr_rssi;
        info->flags |= WIRELESS_INFO_FLAG_HAS_SIGNAL;
        info->noise_level = sr->isr_noise;
        info->flags |= WIRELESS_INFO_FLAG_HAS_NOISE;
        info->quality = sr->isr_intval;
        info->flags |= WIRELESS_INFO_FLAG_HAS_QUALITY;
    }

    return 1;
#endif
#ifdef __OpenBSD__
    struct ifreq ifr;
    struct ieee80211_bssid bssid;
    struct ieee80211_nwid nwid;
    struct ieee80211_nodereq nr;

    struct ether_addr ea;

    int s, len, ibssid, inwid;
    u_int8_t zero_bssid[IEEE80211_ADDR_LEN];

    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        return (0);

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_data = (caddr_t)&nwid;
    (void)strlcpy(ifr.ifr_name, interface, sizeof(ifr.ifr_name));
    inwid = ioctl(s, SIOCG80211NWID, (caddr_t)&ifr);

    memset(&bssid, 0, sizeof(bssid));
    strlcpy(bssid.i_name, interface, sizeof(bssid.i_name));
    ibssid = ioctl(s, SIOCG80211BSSID, &bssid);

    if (ibssid != 0 || inwid != 0) {
        close(s);
        return 0;
    }

    /* NWID */
    {
        if (nwid.i_len <= IEEE80211_NWID_LEN)
            len = nwid.i_len + 1;
        else
            len = IEEE80211_NWID_LEN + 1;

        strncpy(&info->essid[0], nwid.i_nwid, len);
        info->essid[IW_ESSID_MAX_SIZE] = '\0';
        info->flags |= WIRELESS_INFO_FLAG_HAS_ESSID;
    }

    /* Signal strength */
    {
        memset(&zero_bssid, 0, sizeof(zero_bssid));
        if (ibssid == 0 && memcmp(bssid.i_bssid, zero_bssid, IEEE80211_ADDR_LEN) != 0) {
            memcpy(&ea.ether_addr_octet, bssid.i_bssid, sizeof(ea.ether_addr_octet));

            bzero(&nr, sizeof(nr));
            bcopy(bssid.i_bssid, &nr.nr_macaddr, sizeof(nr.nr_macaddr));
            strlcpy(nr.nr_ifname, interface, sizeof(nr.nr_ifname));

            if (ioctl(s, SIOCG80211NODE, &nr) == 0 && nr.nr_rssi) {
                if (nr.nr_max_rssi)
                    info->signal_level_max = IEEE80211_NODEREQ_RSSI(&nr);
                else
                    info->signal_level = nr.nr_rssi;

                info->flags |= WIRELESS_INFO_FLAG_HAS_SIGNAL;
            }
        }
    }

    close(s);
    return 1;
#endif
    return 0;
}

void print_wireless_info(yajl_gen json_gen, char *buffer, const char *interface, const char *format_up, const char *format_down) {
    const char *walk;
    char *outwalk = buffer;
    wireless_info_t info;

    INSTANCE(interface);

    const char *ip_address = get_ip_addr(interface);
    if (ip_address == NULL) {
        START_COLOR("color_bad");
        outwalk += sprintf(outwalk, "%s", format_down);
        goto out;
    }

    if (get_wireless_info(interface, &info)) {
        walk = format_up;
        if (info.flags & WIRELESS_INFO_FLAG_HAS_QUALITY)
            START_COLOR((info.quality < info.quality_average ? "color_degraded" : "color_good"));
        else
            START_COLOR("color_good");
    } else {
        walk = format_down;
        START_COLOR("color_bad");
    }

    for (; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "quality")) {
            if (info.flags & WIRELESS_INFO_FLAG_HAS_QUALITY) {
                if (info.quality_max)
                    outwalk += sprintf(outwalk, "%03d%%", PERCENT_VALUE(info.quality, info.quality_max));
                else
                    outwalk += sprintf(outwalk, "%d", info.quality);
            } else {
                *(outwalk++) = '?';
            }
            walk += strlen("quality");
        }

        if (BEGINS_WITH(walk + 1, "signal")) {
            if (info.flags & WIRELESS_INFO_FLAG_HAS_SIGNAL) {
                if (info.signal_level_max)
                    outwalk += sprintf(outwalk, "%03d%%", PERCENT_VALUE(info.signal_level, info.signal_level_max));
                else
                    outwalk += sprintf(outwalk, "%d dBm", info.signal_level);
            } else {
                *(outwalk++) = '?';
            }
            walk += strlen("signal");
        }

        if (BEGINS_WITH(walk + 1, "noise")) {
            if (info.flags & WIRELESS_INFO_FLAG_HAS_NOISE) {
                if (info.noise_level_max)
                    outwalk += sprintf(outwalk, "%03d%%", PERCENT_VALUE(info.noise_level, info.noise_level_max));
                else
                    outwalk += sprintf(outwalk, "%d dBm", info.noise_level);
            } else {
                *(outwalk++) = '?';
            }
            walk += strlen("noise");
        }

        if (BEGINS_WITH(walk + 1, "essid")) {
            if (info.flags & WIRELESS_INFO_FLAG_HAS_ESSID)
                outwalk += sprintf(outwalk, "%s", info.essid);
            else
                *(outwalk++) = '?';
            walk += strlen("essid");
        }

        if (BEGINS_WITH(walk + 1, "frequency")) {
            if (info.flags & WIRELESS_INFO_FLAG_HAS_FREQUENCY)
                outwalk += sprintf(outwalk, "%1.1f GHz", info.frequency / 1e9);
            else
                *(outwalk++) = '?';
            walk += strlen("frequency");
        }

        if (BEGINS_WITH(walk + 1, "ip")) {
            outwalk += sprintf(outwalk, "%s", ip_address);
            walk += strlen("ip");
        }

#ifdef LINUX
        if (BEGINS_WITH(walk + 1, "bitrate")) {
            char br_buffer[128];

            iw_print_bitrate(br_buffer, sizeof(br_buffer), info.bitrate);

            outwalk += sprintf(outwalk, "%s", br_buffer);
            walk += strlen("bitrate");
        }
#endif
    }

out:
    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
