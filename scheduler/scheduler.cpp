#include "scheduler.h"

#include "epoll_executor.h"

namespace dRPC
{
    Scheduler::Scheduler(int timeout)
    {
        executor_ = std::make_unique<EpollExecutor>(timeout);
    }
}