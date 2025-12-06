#include "accepter.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>

#include "util/common.h"
#include "socket_utils.h"

namespace dRPC::net
{
    Accepter::Accepter(int port, int backlog, int nodelay) : sockfd_(-1), port_(port), backlog_(backlog), nodelay_(nodelay)
    {
        sockfd_ = SocketUtils::socket();

        int opt = 1;
        SocketUtils::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        addr.sin_addr.s_addr = INADDR_ANY;

        SocketUtils::bind(sockfd_, (const struct sockaddr *)&addr, sizeof(addr));

        info("Accepter start listen on port: {}", port_);
        SocketUtils::listen(sockfd_, backlog_);
    }

    Accepter::~Accepter()
    {
        if (sockfd_ != -1)
        {
            ::close(sockfd_);
        }
    }

    void Accepter::set_sock_param(int client_fd)
    {
        if (::fcntl(client_fd, F_SETFL, O_NONBLOCK | O_CLOEXEC) == -1)
        {
            error("fcntl failed: {}", strerror(errno));
            ::close(client_fd);
        }

        // 设置发送缓冲区大小
        int sendbuf = 512 * 1024;
        SocketUtils::setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf));

        // 设置接收缓冲区大小
        int recvbuf = 512 * 1024;
        SocketUtils::setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &recvbuf, sizeof(recvbuf));

        // 设置keepalive
        int keepalive = 1;
        SocketUtils::setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

        // 设置nodelay
        SocketUtils::setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay_, sizeof(nodelay_));
    }

    int Accepter::accept()
    {
        struct sockaddr_in client_addr;
        socklen_t client_addrlen = sizeof(client_addr);
        int client_fd = SocketUtils::accept(sockfd_, (struct sockaddr *)&client_addr, &client_addrlen);

        if (client_fd != -1)
        {
            set_sock_param(client_fd);
        }

        return client_fd;
    }
}