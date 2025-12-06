#pragma once

#include <memory>
#include <coroutine>

#include "util/common.h"
#include "socket.h"
#include "util/chained_buffer.h"
#include "scheduler/awaitable.h"
#include "util/stream.h"

namespace dRPC
{
    class Executor;

    namespace net
    {
        class Connection
        {
        public:
            Connection(int sockfd, Executor *executor, bool dummy = false)
                : is_dummy_(dummy), socket_(std::make_unique<Socket>(sockfd)), executor_(executor) {}
            ~Connection() = default;

            bool is_dummy() const { return is_dummy_; }

            dRPC::ReadAwaiter async_read();
            dRPC::WriteAwaiter async_write();

            Executor *executor() const { return executor_; }

            int fd() const { return socket_->fd(); }
            bool closed() const { return socket_->closed(); }

            void close() { socket_->close(); }

            Socket *socket() const { return socket_.get(); }

            // 设置read/write协程句柄
            void set_read_handle(void *handle) { read_handle_ = handle; }
            void set_write_handle(void *handle) { write_handle_ = handle; }

            // 恢复read/write协程句柄
            void resume_read() const { std::coroutine_handle<>::from_address(read_handle_).resume(); }
            void resume_write() const { std::coroutine_handle<>::from_address(write_handle_).resume(); }

            size_t to_write_bytes() const
            {
                return write_buf_.size();
            }

            size_t to_read_bytes() const
            {
                return read_buf_.size();
            }

            util::InputStream get_input_stream()
            {
                return util::InputStream(&read_buf_);
            }

            util::OutputStream get_output_stream()
            {
                return util::OutputStream(&write_buf_);
            }

        private:
            Executor *executor_;

            bool is_dummy_;
            util::ChainedBuffer<> read_buf_;
            util::ChainedBuffer<> write_buf_;
            void *read_handle_;
            void *write_handle_;
            std::unique_ptr<Socket> socket_;

            Connection(const Connection &) = delete;
            Connection &operator=(const Connection &) = delete;
        };
    }
}