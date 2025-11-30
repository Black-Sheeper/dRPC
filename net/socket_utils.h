#pragma once

#include <string>
#include <netinet/in.h>

namespace net
{
    class SocketUtils
    {
    public:
        static bool SetNonBlocking(int fd, bool non_blocking);
        static bool SetReuseAddr(int fd, bool reuse);
        static bool ResolveHostname(const std::string &hostname, sockaddr_in *addr);
        static std::string GetpeerAddress(int fd);
        static std::string ToString(const sockaddr_in &addr);
    };
}