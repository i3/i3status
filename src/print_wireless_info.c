// vim:ts=8:expandtab
#include <stdio.h>
#include <string.h>

#ifdef LINUX
#include <iwlib.h>
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

        close(skfd);
        return 1;
#endif
}

void print_wireless_info(const char *interface, const char *format_up, const char *format_down) {
        const char *walk;
        wireless_info_t info;
        if (get_wireless_info(interface, &info)) {
                walk = format_up;
                if (info.flags & WIRELESS_INFO_FLAG_HAS_QUALITY)
                        printf("%s", info.quality < info.quality_average ? color("color_degraded") : color("color_good"));
        }
        else {
                walk = format_down;
                printf("%s", color("color_bad"));
        }

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
        }

        (void)printf("%s", endcolor());
}
