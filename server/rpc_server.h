#pragma once

#include <unordered_map>
#include <google/protobuf/service.h>
#include <queue>

#include "net/connection.h"
#include "scheduler/task.h"
#include "net/accepter.h"
#include "scheduler/scheduler.h"

namespace dRPC::server
{
    struct RpcServerOptions
    {
        int port_;
        int backlog_;
        int nodelay_;
        int timeout_;

        RpcServerOptions(int port, int backlog = 256, int nodelay = 1, int timeout = -1)
            : port_(port), backlog_(backlog), nodelay_(nodelay), timeout_(timeout) {}
    };

    class RpcServer
    {
    public:
        RpcServer(const RpcServerOptions &options);
        ~RpcServer() = default;

        void register_service(const std::string &service_name, google::protobuf::Service *service)
        {
            service_registry_[service_name] = service;
        }

        void start();

    private:
        dRPC::Task recv_fn(std::shared_ptr<net::Connection> conn);
        dRPC::Task send_fn(std::shared_ptr<net::Connection> conn);

        RpcServerOptions options_;

        std::queue<std::shared_ptr<net::Connection>> send_queue_;

        net::Accepter accepter_;
        std::unique_ptr<dRPC::Scheduler> scheduler_;

        std::unordered_map<std::string, google::protobuf::Service *> service_registry_;

        RpcServer(const RpcServer &) = delete;
        RpcServer &operator=(const RpcServer &) = delete;
    };
}