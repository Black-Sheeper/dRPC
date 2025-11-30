#pragma once
#include "epoll_executor.h"
#include "task.h"

class ReadAwaitable
{
public:
    ReadAwaitable(int fd, EpollExecutor &executor)
        : fd_(fd), executor_(executor) {}

    bool await_ready()
    {
        return executor_.IsReadReady(fd_);
    }

    void await_suspend(std::coroutine_handle<> coro)
    {
        executor_.RegisterRead(fd_, coro);
    }

    void await_resume() {} // 调用者需要自己读取数据

private:
    int fd_;
    EpollExecutor &executor_;
};

class WriteAwaitable
{
public:
    WriteAwaitable(int fd, EpollExecutor &executor)
        : fd_(fd), executor_(executor) {}

    bool await_ready()
    {
        return executor_.IsWriteReady(fd_);
    }

    void await_suspend(std::coroutine_handle<> coro)
    {
        executor_.RegisterWrite(fd_, coro);
    }

    void await_resume() {} // 调用者需要自己写入数据

private:
    int fd_;
    EpollExecutor &executor_;
};

// 工具函数
inline ReadAwaitable wait_readable(int fd, EpollExecutor &executor)
{
    return ReadAwaitable{fd, executor};
}

inline WriteAwaitable wait_writable(int fd, EpollExecutor &executor)
{
    return WriteAwaitable{fd, executor};
}