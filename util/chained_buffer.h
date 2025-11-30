#pragma once

#include "buffer_block.h"

template <size_t BlockSize = 4096>
class ChainedBuffer
{
private:
    struct Node
    {
        BufferBlock<BlockSize> block;
        Node *next;

        Node() : next(nullptr) {}
    };

public:
    ChainedBuffer() : head_(nullptr), tail_(nullptr), free_list_(nullptr), total_size_(0) {}

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
            if (!tail_ || tail_->block.full())
            {
                append_node();
            }

            size_t to_write = tail_->block.write(src + written, len - written);
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
            BufferBlock<BlockSize> &block = head_->block;

            auto [read_ptr, read_len] = block.read_view();
            size_t to_read = std::min(len - read, read_len);

            memcpy(dest + read, read_ptr, to_read);
            block.read(to_read);

            read += to_read;
            total_size_ -= to_read;

            // 如果当前块空了，移除并回收
            if (block.empty())
            {
                remove_head();
            }
        }

        return read;
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

private:
    Node *head_ = nullptr;
    Node *tail_ = nullptr;
    Node *free_list_ = nullptr;
    size_t total_size_ = 0;

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
};