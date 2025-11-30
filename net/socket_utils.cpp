#include "socket_utils.h"

#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>

namespace net
{
    bool SocketUtils::SetNonBlocking(int fd, bool non_blocking)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0)
            return false;

        if (non_blocking)
        {
            flags |= O_NONBLOCK;
        }
        else
        {
            flags &= ~O_NONBLOCK;
        }

        return fcntl(fd, F_SETFL, flags) == 0;
    }

    bool SocketUtils::SetReuseAddr(int fd, bool reuse)
    {
        int optval = reuse ? 1 : 0;
        return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == 0;
    }

    bool SocketUtils::ResolveHostname(const std::string &hostname, sockaddr_in *addr)
    {
        std::memset(addr, 0, sizeof(sockaddr_in));
        addr->sin_family = AF_INET;

        if (inet_pton(AF_INET, hostname.c_str(), &addr->sin_addr) == 1)
        {
            return true;
        }

        hostent *he = gethostbyname(hostname.c_str());
        if (he == nullptr)
            return false;

        std::memcpy(&addr->sin_addr, he->h_addr_list[0], he->h_length);
        return true;
    }

    std::string SocketUtils::GetpeerAddress(int fd)
    {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        if (getpeername(fd, reinterpret_cast<sockaddr *>(&addr), &len) < 0)
        {
            return "unknown";
        }
        return ToString(addr);
    }

    std::string SocketUtils::ToString(const sockaddr_in &addr)
    {
        char buffer[INET_ADDRSTRLEN];
        const char *result = inet_ntop(AF_INET, &addr.sin_addr, buffer, sizeof(buffer));
        return result ? std::string(result) + ":" + std::to_string(ntohs(addr.sin_port)) : "invalid";
    }
}