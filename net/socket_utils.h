#pragma once

#include <string>
#include <arpa/inet.h>

namespace dRPC::net
{
    class SocketUtils
    {
    public:
        static int socket();

        static void bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

        static void listen(int sockfd, int backlog);

        static int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

        static void inet_pton(int af, const char *src, void *dst);

        static std::string inet_ntoa(struct in_addr in);

        static int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

        static int send(int sockfd, const void *buf, size_t len);

        static void setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
    };
}