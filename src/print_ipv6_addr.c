// vim:ts=4:sw=4:expandtab
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

void print_ipv6_info(yajl_gen json_gen, char *buffer, const char *format_up, const char *format_down) {
    const char *walk;
    char *addr_string = get_ipv6_addr();
    char *outwalk = buffer;

    if (addr_string == NULL) {
        START_COLOR("color_bad");
        outwalk += sprintf(outwalk, "%s", format_down);
        END_COLOR;
        OUTPUT_FULL_TEXT(buffer);
        return;
    }

    START_COLOR("color_good");
    for (walk = format_up; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        if (BEGINS_WITH(walk + 1, "ip")) {
            outwalk += sprintf(outwalk, "%s", addr_string);
            walk += strlen("ip");
        }
    }
    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
