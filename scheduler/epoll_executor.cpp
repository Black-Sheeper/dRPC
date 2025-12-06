#include "epoll_executor.h"

#include <sys/epoll.h>
#include <string.h>
#include <sys/eventfd.h>

#include "util/common.h"

namespace dRPC
{
    EpollExecutor::EpollExecutor(int timeout)
    {
        epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ == -1)
        {
            error("epoll_create1 failed: {}", strerror(errno));
        }

        int event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        info("event_fd: {}", event_fd);
        if (event_fd == -1)
        {
            error("eventfd failed: {}", strerror(errno));
        }

        dummy_conn_ = std::make_unique<dRPC::net::Connection>(event_fd, nullptr, true);

        struct epoll_event ev;
        ev.data.ptr = dummy_conn_.get();
        ev.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, event_fd, &ev) == -1)
        {
            error("epoll_ctl failed: {}", strerror(errno));
        }

        thread_ = std::make_unique<std::thread>([this, timeout]()
                                                {
                                                    while (!stop_)
                                                    {
                                                        Closure task;
                                                        while (task_queue_.pop(task))
                                                        {
                                                            task();
                                                        }

                                                        should_notify_.store(true, std::memory_order_release);
                                                        struct epoll_event events[MAX_EVENTS];
                                                        int nready = epoll_wait(epoll_fd_, events, MAX_EVENTS, timeout);
                                                        if (nready == -1)
                                                        {
                                                            error("epoll_wait failed: {}", strerror(errno));
                                                            continue;
                                                        }
                                                        for (int i = 0; i < nready; ++i)
                                                        {
                                                            auto conn = static_cast<dRPC::net::Connection *>(events[i].data.ptr);
                                                            if (!conn)
                                                            {
                                                                error("conn is null");
                                                                continue;
                                                                ;
                                                            }
                                                            if (conn->is_dummy())
                                                            {
                                                                uint64_t val=1;
                                                                ::read(conn->fd(), &val, sizeof(uint64_t));
                                                                should_notify_.store(false, std::memory_order_release);
                                                                continue;
                                                            }
                                                            if(events[i].events&(EPOLLHUP|EPOLLRDHUP)){
                                                                conn->close();
                                                                if(epoll_ctl(epoll_fd_,EPOLL_CTL_DEL,conn->fd(),nullptr)==-1){
                                                                    error("epoll_ctl failed: {}",strerror(errno));
                                                                }
                                                                continue;
                                                            }
                                                            if(events[i].events&EPOLLOUT){
                                                                struct epoll_event ev;
                                                                ev.data.ptr=conn;
                                                                ev.events=EPOLLIN|EPOLLET;
                                                                if(epoll_ctl(epoll_fd_,EPOLL_CTL_MOD,conn->fd(),&ev)==-1){
                                                                    error("epoll_ctl failed: {}",strerror(errno));
                                                                    continue;
                                                                }
                                                                conn->resume_write();
                                                            }
                                                            if(events[i].events&EPOLLIN){
                                                                conn->resume_read();
                                                            }
                                                        }
                                                    } });
    }

    EpollExecutor::~EpollExecutor()
    {
        if (thread_ && thread_->joinable())
        {
            thread_->join();
        }
        if (epoll_fd_ != -1)
        {
            ::close(epoll_fd_);
            epoll_fd_ = -1;
        }
    }

    void EpollExecutor::stop()
    {
        stop_ = true;
    }

    bool EpollExecutor::spawn(Closure &&task)
    {
        if (!task_queue_.push(std::move(task)))
        {
            error("task queue is full");
            return false;
        }

        bool expect = true;
        if (should_notify_.compare_exchange_strong(expect, false, std::memory_order_release))
        {
            uint64_t val = 1;
            ::write(dummy_conn_->fd(), &val, sizeof(uint64_t));
        }
        return true;
    }

    bool EpollExecutor::add_event(const EventItem &item)
    {
        struct epoll_event ev;
        ev.data.ptr = item.conn;
        switch (item.type)
        {
        case EventType::READ:
            ev.events = EPOLLIN | EPOLLET;
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, item.conn->fd(), &ev) == -1)
            {
                error("epoll_ctl failed: {}", strerror(errno));
                return false;
            }
            break;
        case EventType::WRITE:
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, item.conn->fd(), &ev) == -1)
            {
                error("epoll_ctl failed: {}", strerror(errno));
                return false;
            }
            break;
        case EventType::DELETE:
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, item.conn->fd(), nullptr) == -1)
            {
                error("epoll_ctl failed: {}", strerror(errno));
                return false;
            }
            break;
        case EventType::UNKNOWN:
            ev.events = 0;
            break;
        default:
            error("unknown event type: {}", static_cast<int>(item.type));
            return false;
        }
        return true;
    }
}