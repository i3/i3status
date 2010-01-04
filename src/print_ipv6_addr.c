// vim:ts=8:expandtab
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>

static bool print_sockname(struct addrinfo *addr) {
        static char buf[INET6_ADDRSTRLEN+1];
        struct sockaddr_storage local;
        int ret;
        int fd;

        if ((fd = socket(addr->ai_family, SOCK_DGRAM, 0)) == -1) {
                perror("socket()");
                return false;
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
                return false;
        }


        socklen_t local_len = sizeof(struct sockaddr_storage);
        if (getsockname(fd, (struct sockaddr*)&local, &local_len) == -1) {
                perror("getsockname()");
                (void)close(fd);
                printf("no IPv6");
                return true;
        }

        memset(buf, 0, INET6_ADDRSTRLEN + 1);
        if ((ret = getnameinfo((struct sockaddr*)&local, local_len,
                               buf, sizeof(buf), NULL, 0,
                               NI_NUMERICHOST)) != 0) {
                fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(ret));
                (void)close(fd);
                printf("no IPv6");
                return true;
        }

        (void)close(fd);
        printf("%s", buf);
        return true;
}

/*
 * Returns the IPv6 address with which you have connectivity at the moment.
 *
 */
static void print_ipv6_addr() {
        struct addrinfo hints;
        struct addrinfo *result, *resp;
        static struct addrinfo *cached = NULL;

        /* To save dns lookups (if they are not cached locally) and creating
         * sockets, we save the fd and keep it open. */
        if (cached != NULL)
                if (print_sockname(cached))
                        return;

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET6;

        /* We resolve the K root server to get a public IPv6 address. You can
         * replace this with any other host which has an AAAA record, but the
         * K root server is a pretty safe bet. */
        if (getaddrinfo("k.root-servers.net", "domain", &hints, &result) != 0) {
                /* We don’t display the error here because most
                 * likely, there just is no connectivity.
                 * Thus, don’t spam the user’s console. */
                printf("no IPv6");
                return;
        }

        for (resp = result; resp != NULL; resp = resp->ai_next) {
                if (!print_sockname(resp))
                        continue;

                if ((cached = malloc(sizeof(struct addrinfo))) == NULL)
                        return;
                memcpy(cached, resp, sizeof(struct addrinfo));
                if ((cached->ai_addr = malloc(resp->ai_addrlen)) == NULL) {
                        cached = NULL;
                        return;
                }
                memcpy(cached->ai_addr, resp->ai_addr, resp->ai_addrlen);
                freeaddrinfo(result);
                return;
        }

        freeaddrinfo(result);
        printf("no IPv6");
}

void print_ipv6_info(const char *format) {
        const char *walk;

        for (walk = format; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        putchar(*walk);
                        continue;
                }

                if (strncmp(walk+1, "ip", strlen("ip")) == 0) {
                        print_ipv6_addr();
                        walk += strlen("ip");
                }
        }
}
