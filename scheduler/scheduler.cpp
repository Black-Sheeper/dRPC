#include "scheduler.h"

#include <thread>
#include <chrono>
#include <iostream>

namespace scheduler
{
    Scheduler::Scheduler() : running_(false) {}

    Scheduler::~Scheduler()
    {
        Stop();
    }

    void Scheduler::Schedule(std::coroutine_handle<> handle)
    {
        std::lock_guard lock(mutex_);
        if (handle && !handle.done())
        {
            ready_queue_.push(handle);
        }
    }

    void Scheduler::Schedule(std::function<void()> func)
    {
        std::lock_guard lock(mutex_);
        if (func)
        {
            function_queue_.push(std::move(func));
        }
    }

    void Scheduler::ProcessTasks()
    {
        std::lock_guard lock(mutex_);

        // 处理协程任务
        while (!ready_queue_.empty())
        {
            auto handle = ready_queue_.front();
            ready_queue_.pop();

            if (handle)
            {
                handle.resume();

                if (handle.done())
                {
                    handle.destroy();
                }
            }
        }

        // 处理函数任务
        while (!function_queue_.empty())
        {
            auto func = function_queue_.front();
            function_queue_.pop();
            func();
        }
    }

    void Scheduler::Run()
    {
        std::cout << "[Scheduler] Run started" << std::endl;
        running_ = true;

        while (running_)
        {
            // 先处理IO事件(如果有)
            if (io_executor_.HasEvents())
            {
                io_executor_.Poll(0); // 非阻塞检查IO事件
            }

            // 处理任务队列
            ProcessTasks();

            // 如果还有IO事件等待，继续处理
            if (io_executor_.HasEvents())
            {
                continue;
            }

            // 如果队列为空，等待IO事件或短暂休眠
            {
                std::lock_guard lock(mutex_);
                if (ready_queue_.empty() && function_queue_.empty())
                {
                    if (io_executor_.HasEvents())
                    {
                        // 有IO事件等待，阻塞等待
                        io_executor_.Poll(10);
                    }
                    else
                    {
                        // 没有任务，短暂休眠
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
            }
        }

        std::cout << "[Scheduler] Run exiting" << std::endl;
    }

    void Scheduler::Stop()
    {
        running_ = false;
    }
}