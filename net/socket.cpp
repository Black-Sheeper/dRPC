#include "socket.h"

#include "socket_utils.h"
#include "util/common.h"

namespace dRPC::net
{
    Socket::Socket(int sockfd) : sockfd_(sockfd), closed_(false)
    {
        // 获取本地地址
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        ::getsockname(sockfd, (struct sockaddr *)&addr, &addrlen);
        local_addr_ = SocketUtils::inet_ntoa(addr.sin_addr);
        local_port_ = ::ntohs(addr.sin_port);

        // 获取对端地址
        struct sockaddr_in peer_addr;
        socklen_t peer_addrlen = sizeof(peer_addr);
        ::getpeername(sockfd, (struct sockaddr *)&peer_addr, &peer_addrlen);
        peer_addr_ = SocketUtils::inet_ntoa(peer_addr.sin_addr);
        peer_port_ = ::ntohs(peer_addr.sin_port);

        info("conn [{}]: local: {}:{} <-> peer: {}:{}", sockfd_, local_addr_, local_port_, peer_addr_, peer_port_);
    }

    Socket::~Socket()
    {
        info("close socket: {}", sockfd_);
        ::close(sockfd_);
    }
}