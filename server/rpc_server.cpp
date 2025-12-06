#include "rpc_server.h"

#include "util/common.h"

namespace dRPC::server
{
    RpcServer::RpcServer(const RpcServerOptions &options)
        : options_(options), accepter_(options.port_, options.backlog_, options.nodelay_)
    {
        scheduler_ = std::make_unique<dRPC::Scheduler>(options.timeout_);
    }

    void RpcServer::start()
    {
        while (true)
        {
            int connfd = accepter_.accept();
            if (connfd == -1)
            {
                if (errno != EINTR)
                {
                    error("accept failed: {}", strerror(errno));
                }
                continue;
            }

            auto executor = scheduler_->alloc_executor();
            auto conn = std::make_shared<dRPC::net::Connection>(connfd, executor);

            executor->spawn([this, conn]()
                            { send_fn(conn); });
            executor->spawn([this, conn]()
                            { recv_fn(conn); });
        }
    }

    dRPC::Task RpcServer::recv_fn(std::shared_ptr<net::Connection> conn)
    {
        info("recv_fn belongs to conn: {}", conn->fd());
        co_return;
    }

    dRPC::Task RpcServer::send_fn(std::shared_ptr<net::Connection> conn)
    {
        info("send_fn belongs to conn: {}", conn->fd());
        co_return;
    }
}