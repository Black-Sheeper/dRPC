#pragma once

#include <coroutine>

namespace dRPC
{
    struct Task
    {
        struct promise_type
        {
            Task get_return_object()
            {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            // 初始挂起点
            std::suspend_never initial_suspend() { return {}; }

            // 最终挂起点（协程结束）
            std::suspend_never final_suspend() noexcept { return {}; }

            void unhandled_exception() { std::terminate(); }

            // 无返回值版本
            void return_void() {}
        };

        Task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

        // 恢复协程执行
        void resume()
        {
            if (handle_ && !handle_.done())
            {
                handle_.resume();
            }
        }

        // 销毁协程
        void destroy()
        {
            if (handle_)
            {
                handle_.destroy();
            }
        }

        // 检查是否完成
        bool done() const
        {
            return handle_.done();
        }

    private:
        std::coroutine_handle<promise_type> handle_;

        Task(const Task &) = delete;
        Task &operator=(const Task &) = delete;
    };
}