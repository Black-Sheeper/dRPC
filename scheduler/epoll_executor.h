#pragma once

#include "scheduler.h"
#include "util/mpmc_queue.h"

namespace dRPC
{
    class EpollExecutor : public Executor
    {
    public:
        EpollExecutor(int timeout);
        ~EpollExecutor();

        bool add_event(const EventItem &item) override;

        void stop() override;

        bool spawn(Closure &&task) override;

    private:
        dRPC::util::MPMCQueue<Closure> task_queue_;

        std::atomic<bool> should_notify_{false};
        std::unique_ptr<dRPC::net::Connection> dummy_conn_;

        static const int MAX_EVENTS = 1024;

        std::unique_ptr<std::thread> thread_;
        int epoll_fd_;
        bool stop_;

        EpollExecutor(const EpollExecutor &) = delete;
        EpollExecutor &operator=(const EpollExecutor &) = delete;
    };
}