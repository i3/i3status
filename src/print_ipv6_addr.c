// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

static char *get_sockname(struct addrinfo *addr) {
    static char buf[INET6_ADDRSTRLEN + 1];
    struct sockaddr_storage local;
    int ret;
    int fd;

    if ((fd = socket(addr->ai_family, SOCK_DGRAM, 0)) == -1) {
        perror("socket()");
        return NULL;
    }

    /* Since the socket was created with SOCK_DGRAM, this is
     * actually not establishing a connection or generating
     * any other network traffic. Instead, as a side-effect,
     * it saves the local address with which packets would
     * be sent to the destination. */
    if (connect(fd, addr->ai_addr, addr->ai_addrlen) == -1) {
        /* We don’t display the error here because most
         * likely, there just is no IPv6 connectivity.
         * Thus, don’t spam the user’s console but just
         * try the next address. */
        (void)close(fd);
        return NULL;
    }

    socklen_t local_len = sizeof(struct sockaddr_storage);
    if (getsockname(fd, (struct sockaddr *)&local, &local_len) == -1) {
        perror("getsockname()");
        (void)close(fd);
        return NULL;
    }

    memset(buf, 0, INET6_ADDRSTRLEN + 1);
    if ((ret = getnameinfo((struct sockaddr *)&local, local_len,
                           buf, sizeof(buf), NULL, 0,
                           NI_NUMERICHOST)) != 0) {
        fprintf(stderr, "i3status: getnameinfo(): %s\n", gai_strerror(ret));
        (void)close(fd);
        return NULL;
    }

    (void)close(fd);
    return buf;
}

/*
 * Returns the IPv6 address with which you have connectivity at the moment.
 * The char * is statically allocated and mustn't be freed
 */
static char *get_ipv6_addr(void) {
    struct addrinfo hints;
    struct addrinfo *result, *resp;
    static struct addrinfo *cached = NULL;

    /* To save dns lookups (if they are not cached locally) and creating
     * sockets, we save the fd and keep it open. */
    if (cached != NULL)
        return get_sockname(cached);

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;

    /* We use the public IPv6 of the K root server here. It doesn’t matter
     * which IPv6 address we use (we don’t even send any packets), as long
     * as it’s considered global by the kernel.
     * NB: We don’t use a hostname since that would trigger a DNS lookup.
     * By using an IPv6 address, getaddrinfo() will *not* do a DNS lookup,
     * but return the address in the appropriate struct. */
    if (getaddrinfo("2001:7fd::1", "domain", &hints, &result) != 0) {
        /* We don’t display the error here because most
         * likely, there just is no connectivity.
         * Thus, don’t spam the user’s console. */
        return NULL;
    }

    for (resp = result; resp != NULL; resp = resp->ai_next) {
        char *addr_string = get_sockname(resp);
        /* If we could not get our own address and there is more than
         * one result for resolving k.root-servers.net, we cannot
         * cache. Otherwise, no matter if we got IPv6 connectivity or
         * not, we will cache the (single) result and are done. */
        if (!addr_string && result->ai_next != NULL)
            continue;

        if ((cached = malloc(sizeof(struct addrinfo))) == NULL)
            return NULL;
        memcpy(cached, resp, sizeof(struct addrinfo));
        if ((cached->ai_addr = malloc(resp->ai_addrlen)) == NULL) {
            cached = NULL;
            return NULL;
        }
        memcpy(cached->ai_addr, resp->ai_addr, resp->ai_addrlen);
        freeaddrinfo(result);
        return addr_string;
    }

    freeaddrinfo(result);
    return NULL;
}

/*
 * Returns the name of the interface with which the given IPv6 address is
 * associated.
 * The return value is a statically allocated char* and mustn't be freed.
 */
char *get_iface_addr(const char *searched_addr_string) {
    static char iface_string[IFNAMSIZ] = "(error)";
    struct ifaddrs *addresses;

    if (searched_addr_string == NULL) {
        return iface_string;
    }

    /* getifaddrs(3) returns a linked list of all the available IP addresses on
     * the system. */
    if (getifaddrs(&addresses) == -1) {
        perror("getifaddrs()");
        return iface_string;
    }

    /* Search the linked list for our IP address */
    bool iface_found = false;
    for (struct ifaddrs *addr = addresses; addr != NULL; addr = addr->ifa_next) {
        if (addr->ifa_addr == NULL || addr->ifa_addr->sa_family != AF_INET6) {
            // Not an IPv6 address
            continue;
        }

        /* It's an IPv6 address, cast its `ifa_addr` to a more specialized type */
        struct in6_addr *ipv6_addr = &((struct sockaddr_in6 *)addr->ifa_addr)->sin6_addr;

        /* Get the string representation of the current address */
        char addr_string[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, ipv6_addr, addr_string, sizeof(addr_string)) == NULL) {
            perror("inet_ntop()");
            return iface_string;
        }

        if (!strcmp(addr_string, searched_addr_string)) {
            /* Found the address we wanted */
            iface_found = true;
            strncpy(iface_string, addr->ifa_name, sizeof(iface_string));
            break;
        }
    }
    freeifaddrs(addresses);

    if (!iface_found) {
        fprintf(stderr, "No matching interface found for the outgoing IPv6\n");
    }
    return iface_string;
}

void print_ipv6_info(ipv6_info_ctx_t *ctx) {
    const char *iface_placeholder = "%iface";

    char *addr_string = get_ipv6_addr();
    /* Lookup the interface name only if there is IPv6 connectivity *and* the
     * user actually wants to display the interface name */
    const char *iface_string =
        (addr_string && strstr(ctx->format_up, iface_placeholder))
            ? get_iface_addr(addr_string)
            : "";
    char *outwalk = ctx->buf;

    if (addr_string == NULL) {
        START_COLOR("color_bad");
        outwalk += sprintf(outwalk, "%s", ctx->format_down);
        END_COLOR;
        OUTPUT_FULL_TEXT(ctx->buf);
        return;
    }

    START_COLOR("color_good");

    placeholder_t placeholders[] = {
        {.name = "%ip", .value = addr_string},
        {.name = iface_placeholder, .value = iface_string}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    char *formatted = format_placeholders(ctx->format_up, &placeholders[0], num);
    OUTPUT_FORMATTED;
    free(formatted);

    END_COLOR;
    OUTPUT_FULL_TEXT(ctx->buf);
}
