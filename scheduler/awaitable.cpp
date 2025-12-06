#include "awaitable.h"

#include "scheduler.h"

namespace dRPC
{
    bool RegisterReadAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
    {
        conn_->set_read_handle(handle.address());
        auto executor = conn_->executor();
        executor->add_event({EventType::READ, conn_});
        return false;
    }

    bool ReadAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
    {
        if (conn_->closed())
        {
            auto executor = conn_->executor();
            executor->add_event({EventType::DELETE, conn_});
        }
        return !conn_->closed() && should_suspend_;
    }

    void WriteAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
    {
        conn_->set_write_handle(handle.address());
        auto executor = conn_->executor();
        executor->add_event({EventType::WRITE, conn_});
    }

    bool WaitWriteAwaiter::await_ready() const noexcept
    {
        return conn_->closed() || conn_->to_write_bytes() > 0;
    }

    void WaitWriteAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
    {
        conn_->set_write_handle(handle.address());
    }
}