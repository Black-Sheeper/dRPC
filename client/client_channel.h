#pragma once

#include <string>
#include <google/protobuf/service.h>

#include "scheduler/scheduler.h"
#include "scheduler/task.h"

namespace dRPC
{
    struct ClientOptions
    {
        std::string ip_;
        int port_;
    };

    class ClientChannel : public google::protobuf::RpcChannel
    {
    public:
        ClientChannel(const ClientOptions &options, dRPC::Executor *executor);
        ~ClientChannel() = default;

        void close();

        dRPC::Task recv_fn();
        dRPC::Task send_fn();

        void CallMethod(
            const google::protobuf::MethodDescriptor *method,
            google::protobuf::RpcController *controller,
            const google::protobuf::Message *request,
            google::protobuf::Message *response,
            google::protobuf::Closure *done) override;

    private:
        std::unique_ptr<dRPC::net::Connection> conn_;
        using Session = std::pair<google::protobuf::Message *, google::protobuf::Closure *>;
        std::unordered_map<int64_t, Session> session_registry_;

        dRPC::Executor *executor_;

        int64_t request_id_ = 0;

        ClientChannel(const ClientChannel &) = delete;
        ClientChannel &operator=(const ClientChannel &) = delete;
    };
}