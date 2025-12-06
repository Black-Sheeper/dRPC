#pragma once

#include <vector>
#include <algorithm>
#include <cstring>
#include <sys/uio.h>

#include "buffer_block.h"

namespace dRPC::util
{
    template <size_t BlockSize = 4096>
    class ChainedBuffer
    {
    private:
        struct Node
        {
            util::BufferBlock<BlockSize> block;
            Node *next;

            Node() : next(nullptr) {}
        };

    public:
        ChainedBuffer() : free_list_(nullptr), total_size_(0)
        {
            head_ = new Node();
            tail_ = head_;
        }

        ~ChainedBuffer()
        {
            clear();
            // 释放free_list
            while (free_list_)
            {
                Node *to_delete = free_list_;
                free_list_ = free_list_->next;
                delete to_delete;
            }
        }

        // 写入数据
        size_t write(const void *data, size_t len)
        {
            if (!data || len == 0)
                return 0;

            const char *src = static_cast<const char *>(data);
            size_t written = 0;

            while (written < len)
            {
                util::BufferBlock<BlockSize> &block = tail_->block;

                if (!tail_ || block.full())
                {
                    append_node();
                }

                size_t to_write = block.write(src + written, len - written);
                written += to_write;
                total_size_ += to_write;
            }

            return written;
        };

        // 读取数据
        size_t read(void *buf, size_t len)
        {
            if (!buf || len == 0)
                return 0;

            char *dest = static_cast<char *>(buf);
            size_t read = 0;

            while (head_ && read < len)
            {
                util::BufferBlock<BlockSize> &block = head_->block;

                auto [read_ptr, read_len] = block.read_view();
                size_t to_read = std::min(len - read, read_len);
                std::memcpy(dest + read, read_ptr, to_read);
                block.read(to_read);

                read += to_read;
                consumed_bytes_ += to_read;
                total_size_ -= to_read;

                // 如果当前块空了，移除并回收
                if (block.empty())
                {
                    remove_head();
                }
            }

            return read;
        }

        void commit_resv(int resv)
        {
            if (resv > 0)
            {
                util::BufferBlock<BlockSize> &block = tail_->block;

                block.write_pos += resv;
                total_size_ += resv;
                // 当前块满，分配新块
                if (block.full())
                {
                    append_node();
                }
            }
        }

        void commit_send(size_t send)
        {
            if (send <= 0)
                return;
            total_size_ -= send;

            while (send > 0)
            {
                util::BufferBlock<BlockSize> &block = head_->block;

                int to_send = std::min(send, block.size());
                send -= to_send;
                block.read_pos += to_send;
                if (block.empty())
                {
                    if (!head_->next)
                    {
                        append_node();
                    }

                    remove_head();
                }
            }
        }

        std::pair<char *, size_t> write_view()
        {
            util::BufferBlock<BlockSize> &block = tail_->block;

            return block.write_view();
        }

        std::pair<const char *, size_t> read_view()
        {
            util::BufferBlock<BlockSize> &block = head_->block;

            return block.read_view();
        }

        std::vector<iovec> get_iovecs()
        {
            std::vector<iovec> iovs;
            for (dRPC::util::ChainedBuffer<>::Node* block = head_; block && iovs.size() < IOV_MAX; block = block->next)
            {
                void* data=block->block.data_+block->block.read_pos;
                size_t size=block->block.size();
                iovs.emplace_back(data, size);
            }
            return iovs;
        }

        // 零拷贝遍历
        template <typename F>
        void for_each_block(F &&func) const
        {
            Node *current = head_;
            while (current)
            {
                auto [data, len] = current->block.read_view();
                if (len > 0)
                {
                    func(data, len);
                }
                current = current->next;
            }
        }

        size_t size() const { return total_size_; }
        bool empty() const { return total_size_ == 0; }
        size_t block_count() const
        {
            size_t count = 0;
            Node *current = head_;
            while (current)
            {
                count++;
                current = current->next;
            }
            return count;
        }

        void clear()
        {
            while (head_)
            {
                remove_head();
            }
            total_size_ = 0;
        }

        int64_t input_byte_count() const { return consumed_bytes_; }
        int64_t output_byte_count() const { return total_size_; }

        bool input_next(const void **data, int *size)
        {
            util::BufferBlock<BlockSize> &block = head_->block;

            if (block.empty() || limit_ == 0)
            {
                return false;
            }
            *data = block.data_ + block.read_pos;
            *size = std::min((int)block.size(), limit_);
            limit_ -= limit_ == INT32_MAX ? 0 : *size;
            block.read_pos += *size;
            consumed_bytes_ += *size;
            if (block.read_pos == BlockSize && head_->next)
            {
                remove_head();
            }
            total_size_ -= *size;
            return true;
        }

        void input_back_up(int n)
        {
            util::BufferBlock<BlockSize> &block = head_->block;

            int backup_bytes = std::min(n, (int)block.read_pos);
            block.read_pos -= backup_bytes;
            total_size_ += backup_bytes;
            limit_ += limit_ == INT32_MAX ? 0 : backup_bytes;
            consumed_bytes_ -= backup_bytes;
        }

        bool input_skip(int n)
        {
            util::BufferBlock<BlockSize> &block = head_->block;

            if (n > input_byte_count())
            {
                return false;
            }
            while (n > 0)
            {
                int to_skip = std::min(n, (int)block.size());
                block.read_pos += to_skip;
                n -= to_skip;
                consumed_bytes_ += to_skip;
                total_size_ -= to_skip;

                if (block.read_pos == BlockSize && head_->next)
                {
                    remove_head();
                }
            }
            return true;
        }

        bool output_next(void **data, int *size)
        {
            util::BufferBlock<BlockSize> &block = tail_->block;

            if (block.full())
            {
                append_node();
                block = tail_->block;
            }
            *data = block.data_ + block.write_pos;
            *size = block.available();
            total_size_ += *size;
            block.write_pos += *size;
            return true;
        }

        void output_back_up(int count)
        {
            util::BufferBlock<BlockSize> &block = tail_->block;

            int to_backup = std::min(count, (int)block.write_pos);
            total_size_ -= to_backup;
            block.write_pos -= to_backup;
        }

    private:
        Node *head_;
        Node *tail_;
        Node *free_list_;
        size_t total_size_;
        size_t consumed_bytes_ = 0;

        int limit_ = INT32_MAX;

        void push_limit(int limit)
        {
            limit_ = limit;
        }

        void pop_limit()
        {
            limit_ = INT32_MAX;
        }

        Node *allocate_node()
        {
            if (free_list_)
            {
                Node *node = free_list_;
                free_list_ = free_list_->next;
                node->next = nullptr;
                node->block.reset();
                return node;
            }
            return new Node();
        }

        void deallocate_node(Node *node)
        {
            if (node)
            {
                node->block.reset();
                node->next = free_list_;
                free_list_ = node;
            }
        }

        void append_node()
        {
            Node *new_node = allocate_node();

            if (!tail_)
            {
                head_ = tail_ = new_node;
            }
            else
            {
                tail_->next = new_node;
                tail_ = new_node;
            }
        }

        void remove_head()
        {
            if (head_)
            {
                Node *to_delete = head_;
                head_ = head_->next;
                if (tail_ == to_delete)
                {
                    tail_ = nullptr;
                }
                deallocate_node(to_delete);
            }
        }

        ChainedBuffer(const ChainedBuffer &) = delete;
        ChainedBuffer &operator=(const ChainedBuffer &) = delete;
    };
}