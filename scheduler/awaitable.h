#pragma once

#include <coroutine>

namespace dRPC
{
    namespace net{
        class Connection;
    }
    
    struct RegisterReadAwaiter
    {
        dRPC::net::Connection *conn_;

        bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<> handle) noexcept;
        void await_resume() const noexcept {}
    };

    struct ReadAwaiter
    {
        dRPC::net::Connection *conn_;
        bool should_suspend_;

        bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<> handle) noexcept;
        void await_resume() const noexcept {}
    };

    struct WriteAwaiter
    {
        dRPC::net::Connection *conn_;
        bool should_suspend_;

        bool await_ready() const noexcept { return !should_suspend_; }
        void await_suspend(std::coroutine_handle<> handle) noexcept;
        void await_resume() const noexcept {}
    };

    struct WaitWriteAwaiter
    {
        dRPC::net::Connection *conn_;

        bool await_ready() const noexcept;
        void await_suspend(std::coroutine_handle<> handle) noexcept;
        void await_resume() const noexcept {}
    };
}