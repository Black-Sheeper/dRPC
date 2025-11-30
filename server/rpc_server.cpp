#include "rpc_server.h"

#include <iostream>
#include <chrono>
#include <thread>

namespace server
{
    RpcServer::RpcServer(uint16_t port) : port_(port), running_(false)
    {
        accepter_ = std::make_unique<net::Accepter>(port_, &scheduler_);
        accepter_->SetNewConnectionCallback([this](auto conn)
                                            { HandleNewConnection(conn); });
    }

    RpcServer::~RpcServer()
    {
        Stop();
    }

    void RpcServer::RegisterService(const std::string &service_name, const std::string &method_name, ServiceMethod method)
    {
        std::string full_name = service_name + "." + method_name;
        service_registry_[full_name] = std::move(method);
        std::cout << "Registered RPC method: " << full_name << std::endl;
    }

    void RpcServer::Start()
    {
        if (running_)
        {
            return;
        }

        if (!accepter_->Start())
        {
            throw std::runtime_error("Failed to start RPC server on port " + std::to_string(port_));
        }

        running_ = true;

        // 启动调度器
        std::thread([this]()
                    { scheduler_.Run(); })
            .detach();

        std::cout << "RPC Server started on port " << port_ << std::endl;
    }

    void RpcServer::Stop()
    {
        if (!running_)
        {
            return;
        }

        running_ = false;
        accepter_->Stop();
        scheduler_.Stop();

        std::cout << "RPC Server stopped" << std::endl;
    }

    void RpcServer::HandleNewConnection(std::shared_ptr<net::Connection> conn)
    {
        std::cout << "HandleNewConnection: scheduling connection coroutine for "
                  << conn->GetPeerAddress() << std::endl;
        auto task = HandleConnectionCoroutine(conn);
        scheduler_.Schedule(std::move(task));

        std::cout << "New RPC client connected: " << conn->GetPeerAddress() << std::endl;
    }

    scheduler::Task<> RpcServer::HandleConnectionCoroutine(std::shared_ptr<net::Connection> conn)
    {
        conn->SetCloseCallback([conn]()
                               { std::cout << "RPC client disconnected: " << conn->GetPeerAddress() << std::endl; });

        conn->Start();

        // 处理该连接的所有RPC请求
        while (conn->IsConnected())
        {
            try
            {
                // 使用协程异步读取请求
                auto request_data = co_await conn->AsyncRead();

                if (request_data.empty())
                {
                    std::cout << "Client closed connection: " << conn->GetPeerAddress() << std::endl;
                    break;
                }

                std::cout << "Received RPC request, size: " << request_data.size()
                          << " bytes from " << conn->GetPeerAddress() << std::endl;

                // 将RPC处理交给独立协程，避免阻塞读循环
                auto task = HandleRpcMessageCoroutine(conn, std::move(request_data));
                scheduler_.Schedule(std::move(task));
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error handling RPC connection " << conn->GetPeerAddress() << ": " << e.what() << std::endl;
                break;
            }
        }

        std::cout << "RPC connection coroutine finished: " << conn->GetPeerAddress() << std::endl;
    }

    scheduler::Task<> RpcServer::HandleRpcMessageCoroutine(std::shared_ptr<net::Connection> conn, std::vector<char> request_data)
    {
        try
        {
            // 处理请求
            auto response_data = ProcessRpcCall(request_data);

            if (!response_data.empty())
            {
                // 异步发送响应
                bool success = co_await conn->AsyncWrite(response_data);
                if (success)
                {
                    std::cout << "Sent RPC response, size: " << response_data.size()
                              << " bytes to " << conn->GetPeerAddress() << std::endl;
                }
                else
                {
                    std::cout << "Failed to send response to " << conn->GetPeerAddress() << std::endl;
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "RPC processing error: " << e.what() << std::endl;
        }
    }

    std::vector<char> RpcServer::ProcessRpcCall(const std::vector<char> &request)
    {
        // 简单的echo实现，后面替换为protobuf处理
        std::cout << "Processing RPC call, request size: " << request.size() << " bytes" << std::endl;

        // 这里先简单返回接收到的数据作为响应
        std::string response_msg = "RPC Response to: " + std::to_string(request.size()) + " bytes";
        return std::vector<char>(response_msg.begin(), response_msg.end());
    }
}