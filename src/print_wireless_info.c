// vim:ts=8:expandtab
#include <stdio.h>
#include <string.h>

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

#include "i3status.h"

#define WIRELESS_INFO_FLAG_HAS_ESSID                    (1 << 0)
#define WIRELESS_INFO_FLAG_HAS_QUALITY                  (1 << 1)
#define WIRELESS_INFO_FLAG_HAS_SIGNAL                   (1 << 2)
#define WIRELESS_INFO_FLAG_HAS_NOISE                    (1 << 3)

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

        /* Wireless quality is a relative value in a driver-specific range.
           Signal and noise level can be either relative or absolute values
           in dBm. Furthermore, noise and quality can be expressed directly
           in dBm or in RCPI (802.11k), which we convert to dBm. When those
           values are expressed directly in dBm, they range from -192 to 63,
           and since the values are packed into 8 bits, we need to perform
           8-bit arithmetic on them. Assume absolute values if everything
           else fails (driver bug). */

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
                }
                else {
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
                        }
                        else {
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
        }
        else {
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
#ifdef __FreeBSD__
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
                info->essid[len -1] = '\0';
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
	return 0;
}

void print_wireless_info(const char *interface, const char *format_up, const char *format_down) {
        const char *walk;
        wireless_info_t info;
        if (output_format == O_I3BAR)
                printf("{\"name\":\"wireless\", \"instance\": \"%s\", ", interface);

        if (get_wireless_info(interface, &info)) {
                walk = format_up;
                if (info.flags & WIRELESS_INFO_FLAG_HAS_QUALITY)
                        printf("%s", info.quality < info.quality_average ? color("color_degraded") : color("color_good"));
        }
        else {
                walk = format_down;
                printf("%s", color("color_bad"));
        }

        if (output_format == O_I3BAR)
                printf("\"full_text\":\"");

        for (; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        putchar(*walk);
                        continue;
                }

                if (BEGINS_WITH(walk+1, "quality")) {
                        if (info.flags & WIRELESS_INFO_FLAG_HAS_QUALITY) {
                                if (info.quality_max)
                                        printf("%03d%%", PERCENT_VALUE(info.quality, info.quality_max));
                                else
                                        printf("%d", info.quality);
                        }
                        else {
                                printf("no value");
                        }
                        walk += strlen("quality");
                }

                if (BEGINS_WITH(walk+1, "signal")) {
                        if (info.flags & WIRELESS_INFO_FLAG_HAS_SIGNAL) {
                                if (info.signal_level_max)
                                        printf("%03d%%", PERCENT_VALUE(info.signal_level, info.signal_level_max));
                                else
                                        printf("%d dBm", info.signal_level);
                        }
                        else {
                                printf("no value");
                        }
                        walk += strlen("signal");
                }

                if (BEGINS_WITH(walk+1, "noise")) {
                        if (info.flags & WIRELESS_INFO_FLAG_HAS_NOISE) {
                                if (info.noise_level_max)
                                        printf("%03d%%", PERCENT_VALUE(info.noise_level, info.noise_level_max));
                                else
                                        printf("%d dBm", info.noise_level);
                        }
                        else {
                                printf("no value");
                        }
                        walk += strlen("noise");
                }

                if (BEGINS_WITH(walk+1, "essid")) {
                        if (info.flags & WIRELESS_INFO_FLAG_HAS_ESSID)
                                printf("%s", info.essid);
                        else
                                printf("no value");
                        walk += strlen("essid");
                }

                if (BEGINS_WITH(walk+1, "ip")) {
                        const char *ip_address = get_ip_addr(interface);
                        if (ip_address != NULL)
                                (void)printf("%s", get_ip_addr(interface));
                        else (void)printf("no IP");
                        walk += strlen("ip");
                }

#ifdef LINUX
                if (BEGINS_WITH(walk+1, "bitrate")) {
                        char buffer[128];

                        iw_print_bitrate(buffer, sizeof(buffer), info.bitrate);

                        printf("%s", buffer);
                        walk += strlen("bitrate");
                }
#endif
        }

        (void)printf("%s", endcolor());

        if (output_format == O_I3BAR)
                printf("\"}");
}
