#include "client_channel.h"

#include <fcntl.h>
#include <netinet/tcp.h>

#include "net/socket_utils.h"

namespace dRPC::client
{
    ClientChannel::ClientChannel(const ClientOptions &options, dRPC::Executor *executor)
        : executor_(executor)
    {
        int sockfd = dRPC::net::SocketUtils::socket();

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(options.port_);

        dRPC::net::SocketUtils::inet_pton(AF_INET, options.ip_.c_str(), &addr.sin_addr);
        dRPC::net::SocketUtils::connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr));

        fcntl(sockfd, F_SETFL, O_NONBLOCK | O_CLOEXEC);

        conn_ = std::make_unique<dRPC::net::Connection>(sockfd, executor);

        // 设置发送缓冲区大小
        int sendbuf = 512 * 1024;
        dRPC::net::SocketUtils::setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf));

        // 设置接收缓冲区大小
        int recvbuf = 512 * 1024;
        dRPC::net::SocketUtils::setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recvbuf, sizeof(recvbuf));

        // 设置nodelay
        int nodelay = 1;
        dRPC::net::SocketUtils::setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

        struct linger linger;
        linger.l_onoff = 0;
        linger.l_linger = 0;
        dRPC::net::SocketUtils::setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));

        executor->spawn([this]()
                        { send_fn(); });
        executor->spawn([this]()
                        { recv_fn(); });
    }

    void ClientChannel::close()
    {
        conn_->close();
    }

    dRPC::Task ClientChannel::recv_fn()
    {
        info("recv_fn belongs to conn: {}", conn_->fd());
        co_return;
    }

    dRPC::Task ClientChannel::send_fn()
    {
        info("send_fn belongs to conn: {}", conn_->fd());
        co_return;
    }

    void ClientChannel::CallMethod(
        const google::protobuf::MethodDescriptor *method,
        google::protobuf::RpcController *controller,
        const google::protobuf::Message *request,
        google::protobuf::Message *response,
        google::protobuf::Closure *done)
    {
    }
}