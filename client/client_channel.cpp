#include "client_channel.h"

#include <fcntl.h>
#include <netinet/tcp.h>

#include "net/socket_utils.h"
#include "proto/message.pb.h"

namespace dRPC
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
        // 注册读事件
        co_await RegisterReadAwaiter{conn_.get()};

        while (true)
        {
            auto input_stream = conn_->get_input_stream();

            while (conn_->to_read_bytes() < sizeof(uint32_t) && !conn_->closed())
            {
                co_await conn_->async_read();
            }
            if (conn_->closed() && conn_->to_read_bytes() < sizeof(uint32_t))
            {
                break;
            }
            uint32_t header_len;
            if (!input_stream.read(&header_len, sizeof(uint32_t)))
            {
                error("Failed to read header len");
                break;
            }

            while (conn_->to_read_bytes() < header_len && !conn_->closed())
            {
                co_await conn_->async_read();
            }
            if (conn_->closed() && conn_->to_read_bytes() < header_len)
            {
                break;
            }
            input_stream.push_limit(header_len);
            proto::Header header;
            if (!header.ParseFromZeroCopyStream(&input_stream))
            {
                error("Failed to parse header");
                break;
            }
            input_stream.pop_limit();

            while (conn_->to_read_bytes() < sizeof(uint32_t) && !conn_->closed())
            {
                co_await conn_->async_read();
            }
            if (conn_->closed() && conn_->to_read_bytes() < sizeof(uint32_t))
            {
                break;
            }
            uint32_t response_len;
            if (!input_stream.read(&response_len, sizeof(uint32_t)))
            {
                error("Failed to read response len");
                continue;
            }

            while (conn_->to_read_bytes() < response_len && !conn_->closed())
            {
                co_await conn_->async_read();
            }
            if (conn_->closed() && conn_->to_read_bytes() < response_len)
            {
                break;
            }
            auto request_id = header.request_id();
            auto iter = session_registry_.find(request_id);
            if (iter == session_registry_.end())
            {
                error("Session not found: {}", request_id);
                continue;
            }
            auto [response, done] = iter->second;
            session_registry_.erase(iter);

            input_stream.push_limit(response_len);
            if (!response->ParseFromZeroCopyStream(&input_stream))
            {
                error("Failed to parse response");
                continue;
            }
            input_stream.pop_limit();

            if (done)
            {
                done->Run();
            }
            else
            {
                delete response;
            }
        }

        if (!conn_->closed())
        {
            conn_->close();
        }
    }

    dRPC::Task ClientChannel::send_fn()
    {
        while (!conn_->closed())
        {
            co_await WaitWriteAwaiter{conn_.get()};
            co_await conn_->async_write();
        }
    }

    void ClientChannel::CallMethod(
        const google::protobuf::MethodDescriptor *method,
        google::protobuf::RpcController *controller,
        const google::protobuf::Message *request,
        google::protobuf::Message *response,
        google::protobuf::Closure *done)
    {
        auto send_request = [=, this]
        {
            util::OutputStream output_stream = conn_->get_output_stream();

            proto::Header header;
            header.set_magic(MAGIC_NUM);
            header.set_version(VERSION);
            header.set_message_type(proto::MessageType::REQUEST);
            header.set_request_id(request_id_++);
            header.set_service_name(method->service()->full_name());
            header.set_method_name(method->name());

            uint32_t header_len = header.ByteSizeLong();
            output_stream.write(&header_len, sizeof(header_len));
            header.SerializeToZeroCopyStream(&output_stream);

            uint32_t request_len = request->ByteSizeLong();
            output_stream.write(&request_len, sizeof(request_len));
            request->SerializePartialToZeroCopyStream(&output_stream);

            delete controller;
            delete request;

            session_registry_[header.request_id()] = {response, done};
            conn_->resume_write();
        };
        executor_->spawn(std::move(send_request));
    }
}