#pragma once

#include <functional>
#include <memory>

#include "socket.h"
#include "connection.h"
#include "scheduler.h"

namespace net
{
    class Accepter
    {
    public:
        using NewConnectionCallback = std::function<void(std::shared_ptr<Connection>)>;

        Accepter(uint16_t port, scheduler::Scheduler *scheduler = nullptr);
        ~Accepter();

        bool Start();
        void Stop();

        void SetNewConnectionCallback(NewConnectionCallback cb)
        {
            new_conn_cb_ = std::move(cb);
        }

        bool IsRunning() const { return running_; }
        uint16_t GetPort() const { return port_; }

    private:
        void AcceptLoop();

        std::unique_ptr<Socket> listen_socket_;
        uint16_t port_;
        bool running_;
        NewConnectionCallback new_conn_cb_;
        scheduler::Scheduler *scheduler_;
    };
}