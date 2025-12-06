#pragma once

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <utility>

namespace dRPC::util
{
    template <size_t Capacity>
    class BufferBlock
    {
    public:
        static constexpr size_t capacity_ = Capacity; // 缓冲区容量

        BufferBlock() = default;

        // 容量和状态查询
        constexpr size_t size() const noexcept { return write_pos - read_pos; }
        constexpr static size_t capacity() noexcept { return capacity_; }
        constexpr size_t available() const noexcept { return capacity_ - write_pos; }
        constexpr bool full() const noexcept { return write_pos == capacity_; }
        constexpr bool empty() const noexcept { return read_pos == write_pos; }

        size_t write(const void *src, size_t len)
        {
            len = std::min(len, available());
            if (len > 0)
            {
                std::memcpy(data_ + write_pos, src, len);
                write_pos += len;
            }
            return len;
        }

        size_t read(size_t len)
        {
            len = std::min(len, size());
            read_pos += len;

            if (empty())
            {
                reset();
            }

            return len;
        }

        std::pair<const char *, size_t> read_view()
        {
            if (empty())
                return {nullptr, 0};
            return {data_ + read_pos, write_pos - read_pos};
        }

        std::pair<char *, size_t> write_view()
        {
            if (full())
                return {nullptr, 0};
            return {data_ + write_pos, capacity_ - write_pos};
        }

        void reset()
        {
            read_pos = write_pos = 0;
        }

        char data_[Capacity];
        size_t read_pos = 0;  // 读取位置
        size_t write_pos = 0; // 写入位置
    };
}