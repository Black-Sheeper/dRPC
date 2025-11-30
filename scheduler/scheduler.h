#pragma once

#include <queue>
#include <coroutine>
#include <mutex>
#include <functional>
#include <atomic>

#include "task.h"
#include "epoll_executor.h"

namespace scheduler
{
    class Scheduler
    {
    private:
        void ProcessTasks();

        std::queue<std::coroutine_handle<>> ready_queue_;
        std::queue<std::function<void()>> function_queue_;
        std::mutex mutex_;
        std::atomic<bool> running_;

        scheduler::EpollExecutor io_executor_;

    public:
        Scheduler();
        ~Scheduler();

        // 协程调度
        void Schedule(std::coroutine_handle<> handle);
        void Schedule(std::function<void()> func);

        template <typename T>
        void Schedule(Task<T> task)
        {
            // 获取协程句柄并调度
            auto handle = task.get_handle();
            if (handle && !handle.done())
            {
                std::coroutine_handle<> base = handle;
                Schedule(base);
            }
        }

        // IO事件管理
        scheduler::EpollExecutor &GetIOExecutor() { return io_executor_; }

        // 运行调度器
        void Run();
        void Stop();
        bool Running() const { return running_; }
    };
}