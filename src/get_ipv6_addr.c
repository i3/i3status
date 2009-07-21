// vim:ts=8:expandtab
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>

/*
 * Returns the IPv6 address with which you have connectivity at the moment.
 *
 */
const char *get_ipv6_addr() {
        static char buf[INET6_ADDRSTRLEN+1];
        struct addrinfo hints;
        struct addrinfo *result, *resp;
        int fd;

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET6;

        /* We resolve the K root server to get a public IPv6 address. You can
         * replace this with any other host which has an AAAA record, but the
         * K root server is a pretty safe bet. */
        if (getaddrinfo("k.root-servers.net", "domain", &hints, &result) != 0) {
                perror("getaddrinfo()");
                return "no IP";
        }

        for (resp = result; resp != NULL; resp = resp->ai_next) {
                if ((fd = socket(resp->ai_family, SOCK_DGRAM, 0)) == -1) {
                        perror("socket()");
                        continue;
                }

                /* Since the socket was created with SOCK_DGRAM, this is
                 * actually not establishing a connection or generating
                 * any other network traffic. Instead, as a side-effect,
                 * it saves the local address with which packets would
                 * be sent to the destination. */
                if (connect(fd, resp->ai_addr, resp->ai_addrlen) == -1) {
                        perror("connect()");
                        (void)close(fd);
                        continue;
                }

                struct sockaddr_storage local;
                socklen_t local_len = sizeof(struct sockaddr_storage);
                if (getsockname(fd, (struct sockaddr*)&local, &local_len) == -1) {
                        perror("getsockname()");
                        (void)close(fd);
                        return "no IP";
                }

                (void)close(fd);

                memset(buf, 0, INET6_ADDRSTRLEN + 1);
                int ret;
                if ((ret = getnameinfo((struct sockaddr*)&local, local_len,
                                       buf, sizeof(buf), NULL, 0,
                                       NI_NUMERICHOST)) != 0) {
                        fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(ret));
                        return "no IP";
                }

                free(result);
                return buf;
        }

        free(result);
        return "no IP";
}
