#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <queue>
#include <iostream>
#include <system_error>

#include "socket.h"
#include "epoll_executor.h"
#include "task.h"
#include "awaitable.h"

namespace net
{

    class Connection : public std::enable_shared_from_this<Connection>
    {

    public:
        using ReadCallback = std::function<void(std::vector<char> &&data)>;
        using CloseCallback = std::function<void()>;
        using ErrorCallback = std::function<void(const std::string &error)>;

        Connection(std::unique_ptr<Socket> socket, scheduler::EpollExecutor *executor)
            : socket_(std::move(socket)), executor_(executor), connected_(false)
        {
            if (socket_ && socket_->IsValid())
            {
                connected_ = true;
                socket_->SetNonBlocking(true);
                peer_address_ = socket_->GetPeerAddress();
                if (peer_address_.empty())
                {
                    peer_address_ = "unknown";
                }
            }
            else
            {
                throw std::runtime_error("Invalid socket provided to Connection");
            }
        }

        ~Connection()
        {
            Close();
        }

        // 禁止拷贝
        Connection(const Connection &) = delete;
        Connection &operator=(const Connection &) = delete;

        void Start()
        {
            if (!connected_ || !executor_)
            {
                return;
            }

            std::cout << "Connection started, fd: " << GetFd() << std::endl;
        }

        void Close()
        {
            if (!connected_)
            {
                return;
            }

            connected_ = false;

            if (executor_)
            {
                executor_->UnregisterFd(GetFd());
            }

            if (socket_)
            {
                socket_->Close();
            }

            if (close_cb_)
            {
                close_cb_();
            }

            std::cout << "Connection closed, fd: " << GetFd() << std::endl;
        }

        // 设置回调
        void SetReadCallback(ReadCallback cb) { read_cb_ = std::move(cb); }
        void SetCloseCallback(CloseCallback cb) { close_cb_ = std::move(cb); }
        void SetErrorCallback(ErrorCallback cb) { error_cb_ = std::move(cb); }

        // 同步发送数据
        bool Send(const void *data, size_t length)
        {
            return Send(std::vector<char>(static_cast<const char *>(data), static_cast<const char *>(data) + length));
        }

        bool Send(const std::vector<char> &data)
        {
            if (!connected_ || data.empty())
            {
                return false;
            }

            // 直接尝试写入
            ssize_t n = socket_->Write(data.data(), data.size());
            if (n == static_cast<ssize_t>(data.size()))
            {
                return true;
            }
            else if (n >= 0)
            {
                // 部分写入，将剩余数据加入队列
                std::vector<char> remaining(data.begin() + n, data.end());
                write_queue_.push(std::move(remaining));
                EnableWriting(true);
                return true;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 内核缓冲区满，加入队列等待
                write_queue_.push(data);
                EnableWriting(true);
                return true;
            }
            else
            {
                HandleError("Send failed: " + std::to_string(errno));
                return false;
            }
        }

        bool Send(const std::string &data)
        {
            return Send(data.data(), data.size());
        }

        // 协程异步读取
        scheduler::Task<std::vector<char>> AsyncRead()
        {
            if (!connected_)
            {
                co_return std::vector<char>{};
            }

            // 等待socket可读
            uint32_t events = co_await scheduler::WaitReadable(GetFd(), executor_);

            std::cout << "[Connection] AsyncRead resumed, fd: " << GetFd()
                      << ", events=" << events << std::endl;

            if (!connected_)
            {
                co_return std::vector<char>{};
            }

            // 检查事件类型
            if (events & (EPOLLERR | EPOLLHUP))
            {
                HandleError("Socket error in async read");
                co_return std::vector<char>{};
            }

            if (events & EPOLLRDHUP)
            {
                Close(); // 对端关闭连接
                co_return std::vector<char>{};
            }

            // 读取数据
            char buffer[4096];
            ssize_t n = socket_->Read(buffer, sizeof(buffer));
            if (n > 0)
            {
                std::cout << "[Connection] AsyncRead read " << n << " bytes from fd "
                          << GetFd() << std::endl;
                co_return std::vector<char>(buffer, buffer + n);
            }
            else if (n == 0)
            {
                Close(); // 对端关闭
                co_return std::vector<char>{};
            }
            else
            {
                HandleError("Read failed in async read");
                co_return std::vector<char>{};
            }
        }

        // 协程异步写入
        scheduler::Task<bool> AsyncWrite(const std::vector<char> &data)
        {
            if (!connected_ || data.empty())
            {
                co_return false;
            }

            // 先尝试直接写入
            ssize_t n = socket_->Write(data.data(), data.size());
            if (n == static_cast<ssize_t>(data.size()))
            {
                co_return true; // 全部写入成功
            }

            if (n >= 0)
            {
                // 部分写入，将剩余数据加入队列
                std::vector<char> remaining(data.begin() + n, data.end());
                write_queue_.push(std::move(remaining));
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 内核缓冲区满，加入队列
                write_queue_.push(data);
            }
            else
            {
                HandleError("Write failed in async write");
                co_return false;
            }

            // 等待写入完成，依赖协程 Awaiter 在需要时注册/监听写事件
            while (!write_queue_.empty() && connected_)
            {
                uint32_t events = co_await scheduler::WaitWritable(GetFd(), executor_);

                if (!connected_)
                {
                    co_return false;
                }

                if (events & (EPOLLERR | EPOLLHUP))
                {
                    HandleError("Socket error while waiting for write");
                    co_return false;
                }

                // 尝试写入队列中的数据
                if (!write_queue_.empty())
                {
                    auto &front_data = write_queue_.front();
                    ssize_t written = socket_->Write(front_data.data(), front_data.size());

                    if (written == static_cast<ssize_t>(front_data.size()))
                    {
                        write_queue_.pop();
                    }
                    else if (written > 0)
                    {
                        // 部分写入，更新队列
                        front_data.erase(front_data.begin(), front_data.begin() + written);
                    }
                    // 写入失败会在下一场等待时重试
                }
            }

            co_return connected_;
        }

        // 异步读取指定大小数据
        scheduler::Task<std::vector<char>> AsyncReadExact(size_t size)
        {
            std::vector<char> result;
            result.reserve(size);

            while (result.size() < size && connected_)
            {
                auto data = co_await AsyncRead();
                if (data.empty())
                {
                    break; // 连接关闭或错误
                }

                size_t needed = size - result.size();
                if (data.size() <= needed)
                {
                    result.insert(result.end(), data.begin(), data.end());
                }
                else
                {
                    result.insert(result.end(), data.begin(), data.begin() + needed);
                    // 将多余的数据放回缓冲区
                    read_buffer_.insert(read_buffer_.end(), data.begin() + needed, data.end());
                }
            }

            co_return result;
        }

        // 带超时的异步读取
        scheduler::Task<std::vector<char>> AsyncReadWithTimeout(std::chrono::milliseconds timeout)
        {
            if (!connected_)
            {
                co_return std::vector<char>{};
            }

            // 同时等待数据和超时
            auto read_task = AsyncRead();
            auto timeout_task = scheduler::WaitFor(executor_, timeout);

            // 实现简单的超时逻辑（需要更复杂的实现来处理真正的竞态条件）
            auto data = co_await read_task;
            co_await timeout_task;

            co_return data; // 如果超时先触发，data可能为空
        }

        // 状态查询
        int GetFd() const { return socket_ ? socket_->GetFd() : -1; }
        bool IsConnected() const { return connected_; }
        bool HasPendingWrites() const { return !write_queue_.empty(); }
        const std::string &GetPeerAddress() const { return peer_address_; }

    private:
        void HandleEvents(uint32_t events)
        {
            if (events & (EPOLLERR | EPOLLHUP))
            {
                HandleError("Socket error or hangup");
                return;
            }

            if (events & EPOLLRDHUP)
            {
                std::cout << "Peer closed connection, fd: " << GetFd() << std::endl;
                Close();
                return;
            }

            if (events & EPOLLIN)
            {
                HandleRead();
            }

            if (events & EPOLLOUT)
            {
                HandleWrite();
            }
        }

        void HandleRead()
        {
            if (!connected_)
                return;

            // 边缘触发，读取所有可用数据
            while (true)
            {
                char buffer[8192];
                ssize_t n = socket_->Read(buffer, sizeof(buffer));

                if (n > 0)
                {
                    read_buffer_.insert(read_buffer_.end(), buffer, buffer + n);

                    // 触发回调
                    if (read_cb_)
                    {
                        std::vector<char> data(read_buffer_.begin(), read_buffer_.end());
                        read_cb_(std::move(data));
                        read_buffer_.clear();
                    }
                }
                else if (n == 0)
                {
                    // 对端关闭连接
                    Close();
                    break;
                }
                else
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        break; // 没有更多数据
                    }
                    else
                    {
                        HandleError("Read error: " + std::to_string(errno));
                        break;
                    }
                }
            }
        }

        void HandleWrite()
        {
            if (!connected_ || write_queue_.empty())
            {
                EnableWriting(false);
                return;
            }

            while (!write_queue_.empty())
            {
                auto &data = write_queue_.front();
                ssize_t n = socket_->Write(data.data(), data.size());

                if (n == static_cast<ssize_t>(data.size()))
                {
                    write_queue_.pop();
                }
                else if (n > 0)
                {
                    // 部分写入，更新队列
                    data.erase(data.begin(), data.begin() + n);
                    break;
                }
                else if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break; // 内核缓冲区满
                }
                else
                {
                    HandleError("Write error: " + std::to_string(errno));
                    return;
                }
            }

            // 如果队列空了，取消写监听
            if (write_queue_.empty())
            {
                EnableWriting(false);
            }
        }

        void HandleError(const std::string &error)
        {
            std::cerr << "Connection error (fd: " << GetFd() << "): " << error << std::endl;

            if (error_cb_)
            {
                error_cb_(error);
            }

            Close();
        }

        void EnableWriting(bool enable)
        {
            if (!executor_)
                return;

            uint32_t events = EPOLLIN | EPOLLRDHUP;
            if (enable)
            {
                events |= EPOLLOUT;
            }

            executor_->ModifyFd(GetFd(), events);
        }

        std::unique_ptr<Socket> socket_;
        scheduler::EpollExecutor *executor_;
        bool connected_;
        std::string peer_address_;

        // 回调函数
        ReadCallback read_cb_;
        CloseCallback close_cb_;
        ErrorCallback error_cb_;

        // 缓冲区
        std::vector<char> read_buffer_;
        std::queue<std::vector<char>> write_queue_;
    };
}