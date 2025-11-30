#pragma once

#include <functional>
#include <unordered_map>
#include <memory>
#include <atomic>

#include "accepter.h"
#include "connection.h"
#include "scheduler.h"

namespace server
{
    class RpcServer
    {
    public:
        using ServiceMethod = std::function<std::vector<char>(const std::vector<char> &request)>;

        RpcServer(uint16_t port);
        ~RpcServer();

        // 注册RPC服务方法
        void RegisterService(const std::string &service_name, const std::string &method_name, ServiceMethod method);

        // 启动服务器
        void Start();
        void Stop();

    private:
        scheduler::Task<> HandleConnectionCoroutine(std::shared_ptr<net::Connection> conn);
        void HandleNewConnection(std::shared_ptr<net::Connection> conn);
        scheduler::Task<> HandleRpcMessageCoroutine(std::shared_ptr<net::Connection> conn, std::vector<char> request_data);
        std::vector<char> ProcessRpcCall(const std::vector<char> &request);

        uint16_t port_;
        std::unique_ptr<net::Accepter> accepter_;
        scheduler::Scheduler scheduler_;

        // 服务注册表：service.method -> 处理函数
        std::unordered_map<std::string, ServiceMethod> service_registry_;
        std::atomic<bool> running_;
    };
}