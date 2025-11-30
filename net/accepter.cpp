#include "accepter.h"

#include <iostream>
#include <thread>

namespace net
{
    Accepter::Accepter(uint16_t port, scheduler::Scheduler *scheduler)
        : port_(port), running_(false), scheduler_(scheduler) {}

    Accepter::~Accepter()
    {
        Stop();
    }

    bool Accepter::Start()
    {
        if (running_)
        {
            return true;
        }

        listen_socket_ = std::make_unique<Socket>();

        if (!listen_socket_->Bind(port_))
        {
            std::cerr << "Failed to bind to port " << port_ << std::endl;
            return false;
        }

        if (!listen_socket_->Listen())
        {
            std::cerr << "Failed to listen on port " << port_ << std::endl;
            return false;
        }

        listen_socket_->SetNonBlocking(true);
        listen_socket_->SetReuseAddr(true);

        running_ = true;

        // 启动接收线程
        std::thread([this]()
                    { AcceptLoop(); })
            .detach();

        std::cout << "Accepter started on port " << port_ << std::endl;
        return true;
    }

    void Accepter::Stop()
    {
        if (!running_)
        {
            return;
        }

        running_ = false;

        if (listen_socket_)
        {
            listen_socket_->Close();
        }

        std::cout << "Accepter stopped" << std::endl;
    }

    void Accepter::AcceptLoop()
    {
        while (running_)
        {
            auto client_socket = listen_socket_->Accept();
            if (client_socket && client_socket->IsValid())
            {
                client_socket->SetNonBlocking(true);

                scheduler::EpollExecutor *executor = scheduler_ ? &scheduler_->GetIOExecutor() : nullptr;
                auto connection = std::make_shared<Connection>(std::move(client_socket), executor);

                std::cout << "New connection accepted: " << connection->GetPeerAddress() << std::endl;

                if (new_conn_cb_)
                {
                    new_conn_cb_(connection);
                }
            }
            else
            {
                // 短暂休眠避免CPU占用过高
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
}