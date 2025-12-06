#include "socket_utils.h"

#include <string.h>
#include <unistd.h>

#include "util/common.h"

namespace dRPC::net
{
    int SocketUtils::socket()
    {
        int sockfd;
        if ((sockfd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            error("create socket failed: {}", strerror(errno));
            ::exit(EXIT_FAILURE);
        }
        return sockfd;
    }

    void SocketUtils::bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
    {
        if (::bind(sockfd, addr, addrlen) < 0)
        {
            error("bind failed: {}", strerror(errno));
            ::close(sockfd);
            ::exit(EXIT_FAILURE);
        }
    }

    void SocketUtils::listen(int sockfd, int backlog)
    {
        if (::listen(sockfd, backlog) < 0)
        {
            error("listen failed: {}", strerror(errno));
            ::close(sockfd);
            ::exit(EXIT_FAILURE);
        }
    }

    int SocketUtils::accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
    {
        int client_fd;
        if ((client_fd = ::accept(sockfd, addr, addrlen)) < 0)
        {
            error("accept failed: {}", strerror(errno));
        }
        return client_fd;
    }

    void SocketUtils::inet_pton(int af, const char *src, void *dst)
    {
        if (::inet_pton(af, src, dst) <= 0)
        {
            error("inet_pton failed: {}", strerror(errno));
            ::exit(EXIT_FAILURE);
        }
    }

    std::string SocketUtils::inet_ntoa(struct in_addr in)
    {
        const char *ip = ::inet_ntoa(in);
        return ip;
    }

    int SocketUtils::connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
    {
        int ret = ::connect(sockfd, addr, addrlen);
        if (ret < 0)
        {
            error("connect failed: {}", strerror(errno));
            ::close(sockfd);
            ::exit(EXIT_FAILURE);
        }
        return ret;
    }

    int SocketUtils::send(int sockfd, const void *buf, size_t len)
    {
        int ret = ::send(sockfd, buf, len, 0);
        if (ret < 0)
        {
            error("send failed: {}", strerror(errno));
        }
        return ret;
    }

    void SocketUtils::setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
    {
        if (::setsockopt(sockfd, level, optname, optval, optlen) < 0)
        {
            error("setsockopt failed: {}", strerror(errno));
            ::close(sockfd);
        }
    }
}