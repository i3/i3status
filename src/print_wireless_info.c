// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <sys/socket.h>

#ifdef __linux__
#include <errno.h>
#include <net/if.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/nl80211.h>
#include <linux/if_ether.h>
#define IW_ESSID_MAX_SIZE 32
#endif

#ifdef __APPLE__
#define IW_ESSID_MAX_SIZE 32
#endif

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/ioctl.h>
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
#include <ifaddrs.h>
#include <stdlib.h>
#include <net/if.h>
#include <net/if_media.h>
#include <netproto/802_11/ieee80211.h>
#include <netproto/802_11/ieee80211_ioctl.h>
#include <unistd.h>
#define IW_ESSID_MAX_SIZE IEEE80211_NWID_LEN
#endif

#ifdef __OpenBSD__
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#define IW_ESSID_MAX_SIZE IEEE80211_NWID_LEN
#endif

#ifdef __NetBSD__
#include <sys/types.h>
#include <sys/socket.h>
#include <net80211/ieee80211.h>
#define IW_ESSID_MAX_SIZE IEEE80211_NWID_LEN
#endif

#include "i3status.h"

#define STRING_SIZE 30

#define WIRELESS_INFO_FLAG_HAS_ESSID (1 << 0)
#define WIRELESS_INFO_FLAG_HAS_QUALITY (1 << 1)
#define WIRELESS_INFO_FLAG_HAS_SIGNAL (1 << 2)
#define WIRELESS_INFO_FLAG_HAS_NOISE (1 << 3)
#define WIRELESS_INFO_FLAG_HAS_FREQUENCY (1 << 4)

#define PERCENT_VALUE(value, total) ((int)(value * 100 / (float)total + 0.5f))

typedef struct {
    int flags;
#ifdef IW_ESSID_MAX_SIZE
    char essid[IW_ESSID_MAX_SIZE + 1];
#endif
#ifdef __linux__
    uint8_t bssid[ETH_ALEN];
#endif
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

#if defined(__linux__) || defined(__FreeBSD__)
// Like iw_print_bitrate, but without the dependency on libiw.
static void print_bitrate(char *buffer, int buflen, int bitrate, const char *format_bitrate) {
    const int kilo = 1e3;
    const int mega = 1e6;
    const int giga = 1e9;

    const double rate = bitrate;
    char scale;
    int divisor;

    if (rate >= giga) {
        scale = 'G';
        divisor = giga;
    } else if (rate >= mega) {
        scale = 'M';
        divisor = mega;
    } else {
        scale = 'k';
        divisor = kilo;
    }
    snprintf(buffer, buflen, format_bitrate, rate / divisor, scale);
}
#endif

#ifdef __linux__
// Based on NetworkManager/src/platform/wifi/wifi-utils-nl80211.c
static uint32_t nl80211_xbm_to_percent(int32_t xbm, int32_t divisor) {
#define NOISE_FLOOR_DBM -90
#define SIGNAL_MAX_DBM -20

    xbm /= divisor;
    if (xbm < NOISE_FLOOR_DBM)
        xbm = NOISE_FLOOR_DBM;
    if (xbm > SIGNAL_MAX_DBM)
        xbm = SIGNAL_MAX_DBM;

    return 100 - 70 * (((float)SIGNAL_MAX_DBM - (float)xbm) / ((float)SIGNAL_MAX_DBM - (float)NOISE_FLOOR_DBM));
}

// Based on NetworkManager/src/platform/wifi/wifi-utils-nl80211.c
static void find_ssid(uint8_t *ies, uint32_t ies_len, uint8_t **ssid, uint32_t *ssid_len) {
#define WLAN_EID_SSID 0
    *ssid = NULL;
    *ssid_len = 0;

    while (ies_len > 2 && ies[0] != WLAN_EID_SSID) {
        ies_len -= ies[1] + 2;
        ies += ies[1] + 2;
    }
    if (ies_len < 2)
        return;
    if (ies_len < (uint32_t)(2 + ies[1]))
        return;

    *ssid_len = ies[1];
    *ssid = ies + 2;
}

static int gwi_sta_cb(struct nl_msg *msg, void *data) {
    wireless_info_t *info = data;

    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *sinfo[NL80211_STA_INFO_MAX + 1];
    struct nlattr *rinfo[NL80211_RATE_INFO_MAX + 1];
    static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1] = {
        [NL80211_STA_INFO_RX_BITRATE] = {.type = NLA_NESTED},
    };

    static struct nla_policy rate_policy[NL80211_RATE_INFO_MAX + 1] = {
        [NL80211_RATE_INFO_BITRATE] = {.type = NLA_U16},
    };

    if (nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL) < 0)
        return NL_SKIP;

    if (tb[NL80211_ATTR_STA_INFO] == NULL)
        return NL_SKIP;

    if (nla_parse_nested(sinfo, NL80211_STA_INFO_MAX, tb[NL80211_ATTR_STA_INFO], stats_policy))
        return NL_SKIP;

    if (sinfo[NL80211_STA_INFO_RX_BITRATE] == NULL)
        return NL_SKIP;

    if (nla_parse_nested(rinfo, NL80211_RATE_INFO_MAX, sinfo[NL80211_STA_INFO_RX_BITRATE], rate_policy))
        return NL_SKIP;

    if (rinfo[NL80211_RATE_INFO_BITRATE] == NULL)
        return NL_SKIP;

    // NL80211_RATE_INFO_BITRATE is specified in units of 100 kbit/s, but iw
    // used to specify bit/s, so we convert to use the same code path.
    info->bitrate = (int)nla_get_u16(rinfo[NL80211_RATE_INFO_BITRATE]) * 100 * 1000;

    if (sinfo[NL80211_STA_INFO_SIGNAL] != NULL) {
        info->flags |= WIRELESS_INFO_FLAG_HAS_SIGNAL;
        info->signal_level = (int8_t)nla_get_u8(sinfo[NL80211_STA_INFO_SIGNAL]);

        info->flags |= WIRELESS_INFO_FLAG_HAS_QUALITY;
        info->quality = nl80211_xbm_to_percent(info->signal_level, 1);
        info->quality_max = 100;
        info->quality_average = 50;
    }

    return NL_SKIP;
}

static int gwi_scan_cb(struct nl_msg *msg, void *data) {
    wireless_info_t *info = data;
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct nlattr *bss[NL80211_BSS_MAX + 1];
    struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
        [NL80211_BSS_FREQUENCY] = {.type = NLA_U32},
        [NL80211_BSS_BSSID] = {.type = NLA_UNSPEC},
        [NL80211_BSS_INFORMATION_ELEMENTS] = {.type = NLA_UNSPEC},
        [NL80211_BSS_SIGNAL_MBM] = {.type = NLA_U32},
        [NL80211_BSS_SIGNAL_UNSPEC] = {.type = NLA_U8},
        [NL80211_BSS_STATUS] = {.type = NLA_U32},
    };

    if (nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL) < 0)
        return NL_SKIP;

    if (tb[NL80211_ATTR_BSS] == NULL)
        return NL_SKIP;

    if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS], bss_policy))
        return NL_SKIP;

    if (bss[NL80211_BSS_STATUS] == NULL)
        return NL_SKIP;

    const uint32_t status = nla_get_u32(bss[NL80211_BSS_STATUS]);

    if (status != NL80211_BSS_STATUS_ASSOCIATED &&
        status != NL80211_BSS_STATUS_IBSS_JOINED)
        return NL_SKIP;

    if (bss[NL80211_BSS_BSSID] == NULL)
        return NL_SKIP;

    memcpy(info->bssid, nla_data(bss[NL80211_BSS_BSSID]), ETH_ALEN);

    if (bss[NL80211_BSS_FREQUENCY]) {
        info->flags |= WIRELESS_INFO_FLAG_HAS_FREQUENCY;
        info->frequency = (double)nla_get_u32(bss[NL80211_BSS_FREQUENCY]) * 1e6;
    }

    if (bss[NL80211_BSS_SIGNAL_UNSPEC]) {
        info->flags |= WIRELESS_INFO_FLAG_HAS_SIGNAL;
        info->signal_level = nla_get_u8(bss[NL80211_BSS_SIGNAL_UNSPEC]);
        info->signal_level_max = 100;

        info->flags |= WIRELESS_INFO_FLAG_HAS_QUALITY;
        info->quality = info->signal_level;
        info->quality_max = 100;
        info->quality_average = 50;
    }

    if (bss[NL80211_BSS_SIGNAL_MBM]) {
        info->flags |= WIRELESS_INFO_FLAG_HAS_SIGNAL;
        info->signal_level = (int)nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]) / 100;

        info->flags |= WIRELESS_INFO_FLAG_HAS_QUALITY;
        info->quality = nl80211_xbm_to_percent(nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]), 100);
        info->quality_max = 100;
        info->quality_average = 50;
    }

    if (bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
        uint8_t *ssid;
        uint32_t ssid_len;

        find_ssid(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]),
                  nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]),
                  &ssid, &ssid_len);
        if (ssid && ssid_len) {
            info->flags |= WIRELESS_INFO_FLAG_HAS_ESSID;
            snprintf(info->essid, sizeof(info->essid), "%.*s", ssid_len, ssid);
        }
    }

    return NL_SKIP;
}
#endif

static int get_wireless_info(const char *interface, wireless_info_t *info) {
    memset(info, 0, sizeof(wireless_info_t));

#ifdef __linux__
    struct nl_sock *sk = nl_socket_alloc();
    if (genl_connect(sk) != 0)
        goto error1;

    if (nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM, gwi_scan_cb, info) < 0)
        goto error1;

    const int nl80211_id = genl_ctrl_resolve(sk, "nl80211");
    if (nl80211_id < 0)
        goto error1;

    const unsigned int ifidx = if_nametoindex(interface);
    if (ifidx == 0)
        goto error1;

    struct nl_msg *msg = NULL;
    if ((msg = nlmsg_alloc()) == NULL)
        goto error1;

    if (!genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nl80211_id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0) ||
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifidx) < 0)
        goto error2;

    if (nl_send_sync(sk, msg) < 0)
        // nl_send_sync calls nlmsg_free()
        goto error1;
    msg = NULL;

    if (nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM, gwi_sta_cb, info) < 0)
        goto error1;

    if ((msg = nlmsg_alloc()) == NULL)
        goto error1;

    if (!genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nl80211_id, 0, NLM_F_DUMP, NL80211_CMD_GET_STATION, 0) || nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifidx) < 0 || nla_put(msg, NL80211_ATTR_MAC, 6, info->bssid) < 0)
        goto error2;

    if (nl_send_sync(sk, msg) < 0)
        // nl_send_sync calls nlmsg_free()
        goto error1;
    msg = NULL;

    nl_socket_free(sk);
    return 1;

error2:
    nlmsg_free(msg);
error1:
    nl_socket_free(sk);
    return 0;

#endif
#if defined(__FreeBSD__) || defined(__DragonFly__)
    int s, inwid;
    union {
        struct ieee80211req_sta_req req;
        uint8_t buf[24 * 1024];
    } u;
    struct ieee80211req na;
    char bssid[IEEE80211_ADDR_LEN];
    size_t len;

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
    na.i_type = IEEE80211_IOC_BSSID;
    na.i_data = bssid;
    na.i_len = sizeof(bssid);

    if (ioctl(s, SIOCG80211, (caddr_t)&na) == -1) {
        close(s);
        return (0);
    }

    memcpy(u.req.is_u.macaddr, bssid, sizeof(bssid));
    memset(&na, 0, sizeof(na));
    strlcpy(na.i_name, interface, sizeof(na.i_name));
    na.i_type = IEEE80211_IOC_STA_INFO;
    na.i_data = &u;
    na.i_len = sizeof(u);

    if (ioctl(s, SIOCG80211, (caddr_t)&na) == -1) {
        close(s);
        return (0);
    }

    close(s);
    if (na.i_len >= sizeof(u.req)) {
        /*
	 * Just use the first BSSID returned even if there are
	 * multiple APs sharing the same BSSID.
	 */
        info->signal_level = u.req.info[0].isi_rssi / 2 +
                             u.req.info[0].isi_noise;
        info->flags |= WIRELESS_INFO_FLAG_HAS_SIGNAL;
        info->noise_level = u.req.info[0].isi_noise;
        info->flags |= WIRELESS_INFO_FLAG_HAS_NOISE;
        // isi_txmbps is specified in units of 500 Kbit/s
        // Convert them to bit/s
        info->bitrate = u.req.info[0].isi_txmbps * 500 * 1000;
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

        strncpy(&info->essid[0], (char *)nwid.i_nwid, len);
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
                info->signal_level = nr.nr_rssi;
                if (nr.nr_max_rssi) {
                    info->signal_level_max = nr.nr_max_rssi;
                    info->quality = IEEE80211_NODEREQ_RSSI(&nr);
                } else {
                    if (info->signal_level <= -100)
                        info->quality = 0;
                    else if (info->signal_level > -50)
                        info->quality = 100;
                    else
                        info->quality = abs(2 * (info->signal_level + 100));
                }
                info->quality_max = 100;
                info->quality_average = 50;

                info->flags |= WIRELESS_INFO_FLAG_HAS_QUALITY;
                info->flags |= WIRELESS_INFO_FLAG_HAS_SIGNAL;
            }
        }
    }

    close(s);
    return 1;
#endif
    return 0;
}

/* Table summarizing what is the decision to prefer IPv4 or IPv6
 * based their values.
 *
 * | ipv4_address | ipv6_address | Chosen IP | Color             |
 * |--------------|--------------|-----------|-------------------|
 * | NULL         | NULL         | None      | bad (red)         |
 * | NULL         | no IP        | IPv6      | degraded (orange) |
 * | NULL         | ::1/128      | IPv6      | ok (green)        |
 * | no IP        | NULL         | IPv4      | degraded          |
 * | no IP        | no IP        | IPv4      | degraded          |
 * | no IP        | ::1/128      | IPv6      | ok                |
 * | 127.0.0.1    | NULL         | IPv4      | ok                |
 * | 127.0.0.1    | no IP        | IPv4      | ok                |
 * | 127.0.0.1    | ::1/128      | IPv4      | ok                |
 */
void print_wireless_info(wireless_info_ctx_t *ctx) {
    const char *walk;
    char *outwalk = ctx->buf;
    wireless_info_t info;

    INSTANCE(ctx->interface);

    char *ipv4_address = sstrdup(get_ip_addr(ctx->interface, AF_INET));
    char *ipv6_address = sstrdup(get_ip_addr(ctx->interface, AF_INET6));

    /*
     * Removing '%' and following characters from IPv6 since the interface identifier is redundant,
     * as the output already includes the interface name.
    */
    if (ipv6_address != NULL) {
        char *prct_ptr = strstr(ipv6_address, "%");
        if (prct_ptr != NULL) {
            *prct_ptr = '\0';
        }
    }

    bool prefer_ipv4 = true;
    if (ipv4_address == NULL) {
        if (ipv6_address == NULL) {
            START_COLOR("color_bad");
            outwalk += sprintf(outwalk, "%s", ctx->format_down);

            END_COLOR;
            free(ipv4_address);
            free(ipv6_address);
            OUTPUT_FULL_TEXT(ctx->buf);
            return;
        } else {
            prefer_ipv4 = false;
        }
    } else if (BEGINS_WITH(ipv4_address, "no IP") && ipv6_address != NULL && !BEGINS_WITH(ipv6_address, "no IP")) {
        prefer_ipv4 = false;
    }

    const char *ip_address = (prefer_ipv4) ? ipv4_address : ipv6_address;
    if (!get_wireless_info(ctx->interface, &info)) {
        walk = ctx->format_down;
        START_COLOR("color_bad");
    } else {
        walk = ctx->format_up;
        if (info.flags & WIRELESS_INFO_FLAG_HAS_QUALITY)
            START_COLOR((info.quality < info.quality_average ? "color_degraded" : "color_good"));
        else {
            if (BEGINS_WITH(ip_address, "no IP")) {
                START_COLOR("color_degraded");
            } else {
                START_COLOR("color_good");
            }
        }
    }

    char string_quality[STRING_SIZE] = {'\0'};
    char string_signal[STRING_SIZE] = {'\0'};
    char string_noise[STRING_SIZE] = {'\0'};
    char string_essid[STRING_SIZE] = {'\0'};
    char string_frequency[STRING_SIZE] = {'\0'};
    char string_ip[STRING_SIZE] = {'\0'};
    char string_bitrate[STRING_SIZE] = {'\0'};

    if (info.flags & WIRELESS_INFO_FLAG_HAS_QUALITY) {
        if (info.quality_max)
            snprintf(string_quality, STRING_SIZE, ctx->format_quality, PERCENT_VALUE(info.quality, info.quality_max), pct_mark);
        else
            snprintf(string_quality, STRING_SIZE, "%d", info.quality);
    } else {
        snprintf(string_quality, STRING_SIZE, "?");
    }

    if (info.flags & WIRELESS_INFO_FLAG_HAS_SIGNAL) {
        if (info.signal_level_max)
            snprintf(string_signal, STRING_SIZE, ctx->format_signal, PERCENT_VALUE(info.signal_level, info.signal_level_max), pct_mark);
        else
            snprintf(string_signal, STRING_SIZE, "%d dBm", info.signal_level);
    } else {
        snprintf(string_signal, STRING_SIZE, "?");
    }

    if (info.flags & WIRELESS_INFO_FLAG_HAS_NOISE) {
        if (info.noise_level_max)
            snprintf(string_noise, STRING_SIZE, ctx->format_noise, PERCENT_VALUE(info.noise_level, info.noise_level_max), pct_mark);
        else
            snprintf(string_noise, STRING_SIZE, "%d dBm", info.noise_level);
    } else {
        snprintf(string_noise, STRING_SIZE, "?");
    }

    char *tmp = string_essid;
#ifdef IW_ESSID_MAX_SIZE
    if (info.flags & WIRELESS_INFO_FLAG_HAS_ESSID)
        maybe_escape_markup(info.essid, &tmp);
    else
#endif
        snprintf(string_essid, STRING_SIZE, "?");

    if (info.flags & WIRELESS_INFO_FLAG_HAS_FREQUENCY)
        snprintf(string_frequency, STRING_SIZE, "%1.1f GHz", info.frequency / 1e9);
    else
        snprintf(string_frequency, STRING_SIZE, "?");

    snprintf(string_ip, STRING_SIZE, "%s", ip_address);

#if defined(__linux__) || defined(__FreeBSD__)
    print_bitrate(string_bitrate, sizeof(string_bitrate), info.bitrate, ctx->format_bitrate);
#endif

    placeholder_t placeholders[] = {
        {.name = "%quality", .value = string_quality},
        {.name = "%signal", .value = string_signal},
        {.name = "%noise", .value = string_noise},
        {.name = "%essid", .value = string_essid},
        {.name = "%frequency", .value = string_frequency},
        {.name = "%ip", .value = string_ip},
        {.name = "%bitrate", .value = string_bitrate}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    char *formatted = format_placeholders(walk, &placeholders[0], num);
    OUTPUT_FORMATTED;
    free(formatted);

    END_COLOR;
    free(ipv4_address);
    free(ipv6_address);
    OUTPUT_FULL_TEXT(ctx->buf);
}
