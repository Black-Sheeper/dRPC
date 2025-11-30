#include "socket.h"

#include <stdexcept>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstring>

#include "socket_utils.h"

namespace net
{
    Socket::Socket() : fd_(::socket(AF_INET, SOCK_STREAM, 0))
    {
        if (fd_ < 0)
        {
            throw std::runtime_error("Failed to create socket");
        }
    }

    Socket::Socket(int fd) : fd_(fd) {}

    Socket::~Socket()
    {
        if (fd_ >= 0)
        {
            Close();
        }
    }

    Socket::Socket(Socket &&other) noexcept : fd_(other.fd_)
    {
        other.fd_ = -1;
    }

    Socket &Socket::operator=(Socket &&other) noexcept
    {
        if (this != &other)
        {
            if (fd_ >= 0)
            {
                Close();
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    bool Socket::Bind(uint16_t port)
    {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        return ::bind(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0;
    }

    bool Socket::Listen(int backlog)
    {
        return ::listen(fd_, backlog) == 0;
    }

    std::unique_ptr<Socket> Socket::Accept()
    {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = ::accept(fd_, reinterpret_cast<sockaddr *>(&client_addr), &addr_len);
        if (client_fd < 0)
        {
            return nullptr;
        }

        return std::make_unique<Socket>(client_fd);
    }

    bool Socket::Connect(const std::string &host, uint16_t port)
    {
        sockaddr_in addr{};
        if (!SocketUtils::ResolveHostname(host, &addr))
        {
            return false;
        }
        addr.sin_port = htons(port);

        return ::connect(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0;
    }

    ssize_t Socket::Read(void *buffer, size_t length)
    {
        return ::read(fd_, buffer, length);
    }

    ssize_t Socket::Write(const void *buffer, size_t length)
    {
        return ::write(fd_, buffer, length);
    }

    void Socket::SetNonBlocking(bool non_blocking)
    {
        SocketUtils::SetNonBlocking(fd_, non_blocking);
    }

    void Socket::SetReuseAddr(bool reuse)
    {
        SocketUtils::SetReuseAddr(fd_, reuse);
    }

    std::string Socket::GetPeerAddress() const
    {
        if (fd_ < 0)
        {
            return {};
        }

        sockaddr_storage addr{};
        socklen_t addr_len = sizeof(addr);
        if (::getpeername(fd_, reinterpret_cast<sockaddr *>(&addr), &addr_len) != 0)
        {
            return {};
        }

        char host[INET6_ADDRSTRLEN] = {0};
        uint16_t port = 0;

        if (addr.ss_family == AF_INET)
        {
            auto *in = reinterpret_cast<sockaddr_in *>(&addr);
            if (!::inet_ntop(AF_INET, &in->sin_addr, host, sizeof(host)))
            {
                return {};
            }
            port = ntohs(in->sin_port);
        }
        else if (addr.ss_family == AF_INET6)
        {
            auto *in6 = reinterpret_cast<sockaddr_in6 *>(&addr);
            if (!::inet_ntop(AF_INET6, &in6->sin6_addr, host, sizeof(host)))
            {
                return {};
            }
            port = ntohs(in6->sin6_port);
        }
        else
        {
            return {};
        }

        return std::string(host) + ":" + std::to_string(port);
    }

    void Socket::Close()
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
    }
}