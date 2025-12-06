#pragma once

#include <cstdint>

#include "util/common.h"
#include "net/connection.h"

namespace dRPC
{
    namespace net{
        class Connection;
    }
    
    enum struct EventType : uint8_t
    {
        READ,
        WRITE,
        DELETE,
        UNKNOWN,
    };

    struct EventItem
    {
        EventType type;
        dRPC::net::Connection *conn;
    };

    class Executor
    {
    public:
        Executor() = default;
        virtual ~Executor() = default;

        virtual bool add_event(const EventItem &item) = 0;

        virtual void stop() = 0;

        virtual bool spawn(Closure &&task) = 0;
    };

    class Scheduler
    {
    public:
        Scheduler(int timeout);
        ~Scheduler() = default;

        void stop()
        {
            executor_->stop();
        }

        Executor *alloc_executor() { return executor_.get(); }

    private:
        std::unique_ptr<Executor> executor_;

        Scheduler(const Scheduler &) = delete;
        Scheduler &operator=(const Scheduler &) = delete;
    };
}