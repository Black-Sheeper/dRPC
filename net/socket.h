#pragma once

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <memory>
#include <string>

namespace net
{
    class Socket
    {
    private:
        int fd_;

    public:
        Socket();
        explicit Socket(int fd);
        virtual ~Socket();

        // 禁止拷贝
        Socket(const Socket &) = delete;
        Socket &operator=(const Socket &) = delete;

        // 允许移动
        Socket(Socket &&other) noexcept;
        Socket &operator=(Socket &&other) noexcept;

        bool Bind(uint16_t port);
        bool Listen(int backlog = SOMAXCONN);
        std::unique_ptr<Socket> Accept();
        bool Connect(const std::string &host, uint16_t port);

        ssize_t Read(void *buffer, size_t length);
        ssize_t Write(const void *buffer, size_t length);

        void SetNonBlocking(bool non_blocking);
        void SetReuseAddr(bool reuse);

        std::string GetPeerAddress() const;

        int GetFd() const { return fd_; }
        bool IsValid() const { return fd_ >= 0; }
        void Close();
    };
}