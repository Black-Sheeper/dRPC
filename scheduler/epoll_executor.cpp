#include "epoll_executor.h"

#include <fcntl.h>
#include <poll.h>

EpollExecutor::EpollExecutor() : epoll_fd_(-1), running_(false)
{
    CreateEpoll();
    events_.resize(MAX_EVENTS);
}

EpollExecutor::~EpollExecutor()
{
    Stop();
    if (epoll_fd_ >= 0)
    {
        close(epoll_fd_);
    }
}

void EpollExecutor::CreateEpoll()
{
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0)
    {
        throw std::system_error(errno, std::system_category(), "epoll_create1 failed");
    }
}

void EpollExecutor::RegisterRead(int fd, std::coroutine_handle<> coro)
{
    std::lock_guard lock(mutex_);

    if (read_waiters_.count(fd))
    {
        throw std::runtime_error("FD already registered for reading");
    }

    // 设置非阻塞
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    read_waiters_[fd] = coro;
    UpdataEpoll(fd, EPOLLIN | EPOLLET | EPOLLONESHOT);

    std::cout << "Registered read for fd " << fd << std::endl;
    cv_.notify_all(); // 通知事件循环有新操作
}

void EpollExecutor::RegisterWrite(int fd, std::coroutine_handle<> coro)
{
    std::lock_guard lock(mutex_);

    if (write_waiters_.count(fd))
    {
        throw std::runtime_error("FD already registered for writing");
    }

    // 设置非阻塞
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    write_waiters_[fd] = coro;
    UpdataEpoll(fd, EPOLLOUT | EPOLLET | EPOLLONESHOT);

    std::cout << "Registered write for fd " << fd << std::endl;
    cv_.notify_all(); // 通知事件循环有新操作
}

void EpollExecutor::UnregisterFd(int fd)
{
    std::lock_guard lock(mutex_);

    read_waiters_.erase(fd);
    write_waiters_.erase(fd);

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0 && errno != ENOENT)
    {
        // ENOENT表示fd已经被移除，这是正常的
        std::cerr << "Warning: epoll_ctl DEL failed for fd " << fd
                  << ": " << strerror(errno) << std::endl;
    }

    std::cout << "Unregistered fd " << fd << std::endl;
}

void EpollExecutor::UpdataEpoll(int fd, uint32_t events)
{
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    int op = (read_waiters_.count(fd) || write_waiters_.count(fd)) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

    if (epoll_ctl(epoll_fd_, op, fd, &ev) < 0)
    {
        throw std::system_error(errno, std::system_category(), "epoll_ctl failed");
    }
}

bool EpollExecutor::IsReadReady(int fd) const
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLPRI;
    pfd.revents = 0;
    int ret = ::poll(&pfd, 1, 0);
    return (ret > 0) && (pfd.revents & (POLLIN | POLLPRI));
}

bool EpollExecutor::IsWriteReady(int fd) const
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;
    pfd.revents = 0;
    int ret = ::poll(&pfd, 1, 0);
    return (ret > 0) && (pfd.revents & POLLOUT);
}

void EpollExecutor::WaitForNewOperations()
{
    std::unique_lock lock(mutex_);
    if (read_waiters_.empty() && write_waiters_.empty())
    {
        std::cout << "Waiting for new operations..." << std::endl;
        cv_.wait_for(lock, std::chrono::milliseconds(100));
    }
}

void EpollExecutor::ProcessEvent(const epoll_event &event)
{
    int fd = event.data.fd;
    uint32_t revents = event.events;

    // 处理错误事件
    if (revents & (EPOLLERR | EPOLLHUP))
    {
        std::cerr << "Epoll error event on fd " << fd
                  << ", events: " << revents << std::endl;
        UnregisterFd(fd);
        return;
    }

    // 收集需要恢复的协程
    auto coroutines = [&]() -> std::vector<std::coroutine_handle<>>
    {
        std::lock_guard lock(mutex_);
        std::vector<std::coroutine_handle<>> result;

        // 收集读等待者
        if ((revents & EPOLLIN))
        {
            if (auto it = read_waiters_.find(fd); it != read_waiters_.end())
            {
                result.push_back(it->second);
                read_waiters_.erase(it);
            }
        }

        // 收集写等待者
        if ((revents & EPOLLOUT))
        {
            if (auto it = write_waiters_.find(fd); it != write_waiters_.end())
            {
                result.push_back(it->second);
                write_waiters_.erase(it);
            }
        }

        return result;
    }();

    // 恢复所有协程
    for (auto &coro : coroutines)
    {
        if (coro && !coro.done())
        {
            coro.resume();
        }
    }

    // 如果没有等待者了，移除epoll监控
    std::lock_guard lock(mutex_);
    if (!read_waiters_.count(fd) && !write_waiters_.count(fd))
    {
        UnregisterFd(fd);
    }
}

void EpollExecutor::Poll(int timeout_ms)
{
    int ready_count = epoll_wait(epoll_fd_, events_.data(), MAX_EVENTS, timeout_ms);

    if (ready_count < 0)
    {
        if (errno == EINTR)
        {
            return; // 被信号中断，正常返回
        }
        throw std::system_error(errno, std::system_category(), "epoll_wait failed");
    }

    for (int i = 0; i < ready_count; ++i)
    {
        ProcessEvent(events_[i]);
    }
}

void EpollExecutor::Run()
{
    running_ = true;
    std::cout << "Event loop started" << std::endl;

    while (running_)
    {
        // 先检查是否有操作需要处理
        if (HasPendingOperations())
        {
            Poll(10); // 有操作时快速轮询
        }
        else
        {
            // 没有操作时等待新操作注册
            WaitForNewOperations();
        }
    }

    std::cout << "Event loop stopped" << std::endl;
}

void EpollExecutor::Stop()
{
    std::cout << "Stopping event loop" << std::endl;
    running_ = false;
    cv_.notify_all(); // 唤醒等待的线程
}