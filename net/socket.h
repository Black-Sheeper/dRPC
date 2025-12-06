#pragma once

#include <string>
#include <unistd.h>
#include <sys/socket.h>

namespace dRPC::net
{
    class Socket
    {
    public:
        Socket(int sockfd);
        ~Socket();

        int fd() const { return sockfd_; }
        const std::string &local_addr() const { return local_addr_; }
        int local_port() const { return local_port_; }
        const std::string &peer_addr() const { return peer_addr_; }
        int peer_port() const { return peer_port_; }

        void close()
        {
            if (closed_)
            {
                return;
            }
            ::shutdown(sockfd_, SHUT_WR);
            closed_ = true;
        }

        bool closed() const { return closed_; }

    private:
        int sockfd_;

        std::string local_addr_; // 本地地址
        int local_port_;         // 本地端口
        std::string peer_addr_;  // 对端地址
        int peer_port_;          // 对端端口

        bool closed_; // 关闭标识
    };
}