#pragma once

#include <sys/epoll.h>
#include <unordered_map>
#include <vector>
#include <coroutine>
#include <system_error>
#include <unistd.h>
#include <iostream>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstring>

class EpollExecutor
{
public:
    EpollExecutor();
    ~EpollExecutor();

    // 协程专用接口
    void RegisterRead(int fd, std::coroutine_handle<> coro);
    void RegisterWrite(int fd, std::coroutine_handle<> coro);
    void UnregisterFd(int fd);

    // 状态查询
    bool IsReadReady(int fd) const;
    bool IsWriteReady(int fd) const;

    // 事件循环
    void Poll(int timeout_ms = -1);
    void Run();  // 持续运行事件循环
    void Stop(); // 停止事件循环

    // 准确的状态查询
    bool HasPendingOperations() const
    {
        std::lock_guard lock(mutex_);
        return !read_waiters_.empty() || !write_waiters_.empty();
    }

    // 等待新操作注册
    void WaitForNewOperations();

private:
    void CreateEpoll();
    void UpdataEpoll(int fd, uint32_t events);
    void ProcessEvent(const epoll_event &event);

    int epoll_fd_;
    std::atomic<bool> running_{false};

    // 线程安全的等待者管理
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    // 存储协程句柄
    std::unordered_map<int, std::coroutine_handle<>> read_waiters_;
    std::unordered_map<int, std::coroutine_handle<>> write_waiters_;

    std::vector<epoll_event> events_;
    static const int MAX_EVENTS = 1024;
};