#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <optional>
#include <type_traits>
#include <new>

namespace dRPC::util
{
    template <typename T>
    class MPMCQueue
    {
    private:
        struct Node
        {
            std::atomic<Node *> next;
            typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
            std::atomic<bool> occupied;

            Node() : next(nullptr), occupied(false) {}

            ~Node()
            {
                if (occupied.load(std::memory_order_acquire))
                {
                    destroy();
                }
            }

            T *data_ptr()
            {
                return reinterpret_cast<T *>(&storage);
            }

            void construct(T &&value)
            {
                new (&storage) T(std::move(value));
                occupied.store(true, std::memory_order_release);
            }

            void destroy()
            {
                data_ptr()->~T();
                occupied.store(false, std::memory_order_release);
            }
        };

        struct Chunk
        {
            std::atomic<Chunk *> next;
            Node *nodes;
            size_t size;
            std::atomic<size_t> push_index;
            std::atomic<size_t> pop_index;
            std::atomic<size_t> active_readers;
            std::atomic<bool> retired;

            Chunk(size_t chunk_size) : next(nullptr), size(chunk_size), push_index(0), pop_index(0)
            {
                nodes = new Node[chunk_size];
                active_readers.store(0, std::memory_order_relaxed);
                retired.store(false, std::memory_order_relaxed);
            }

            ~Chunk()
            {
                delete[] nodes;
            }

            // 禁用拷贝
            Chunk(const Chunk &) = delete;
            Chunk &operator=(const Chunk &) = delete;
        };

        static const size_t CHUNK_SIZE = 64; // 缓存友好的块大小

        alignas(64) std::atomic<Chunk *> head_chunk;
        alignas(64) std::atomic<Chunk *> tail_chunk;
        alignas(64) std::atomic<Chunk *> free_list;
        std::atomic<size_t> chunk_count;

    public:
        MPMCQueue() : head_chunk(nullptr), tail_chunk(nullptr), free_list(nullptr), chunk_count(0)
        {
            // 创建初始块
            Chunk *initial_chunk = new Chunk(CHUNK_SIZE);
            head_chunk.store(initial_chunk, std::memory_order_relaxed);
            tail_chunk.store(initial_chunk, std::memory_order_relaxed);
            chunk_count.store(1, std::memory_order_relaxed);
        }

        ~MPMCQueue()
        {
            // 清理所有块
            Chunk *current = head_chunk.load(std::memory_order_relaxed);
            while (current)
            {
                Chunk *next = current->next.load(std::memory_order_relaxed);
                delete current;
                current = next;
            }

            // 清理空闲列表
            current = free_list.load(std::memory_order_relaxed);
            while (current)
            {
                Chunk *next = current->next.load(std::memory_order_relaxed);
                delete current;
                current = next;
            }
        }

        // 禁用拷贝和移动
        MPMCQueue(const MPMCQueue &) = delete;
        MPMCQueue &operator=(const MPMCQueue &) = delete;
        MPMCQueue(MPMCQueue &&) = delete;
        MPMCQueue &operator=(MPMCQueue &&) = delete;

        bool push(T value)
        {
            while (true)
            {
                Chunk *tail = tail_chunk.load(std::memory_order_acquire);
                size_t index = tail->push_index.fetch_add(1, std::memory_order_relaxed);

                if (index < tail->size)
                {
                    // 当前块还有空间
                    Node &node = tail->nodes[index];
                    if (!node.occupied.load(std::memory_order_acquire))
                    {
                        node.construct(std::move(value));
                        return true;
                    }
                }
                else
                {
                    // 当前块已满，需要新块
                    Chunk *new_chunk = allocate_chunk();
                    Chunk *expected = nullptr;
                    if (tail->next.compare_exchange_strong(expected, new_chunk, std::memory_order_release, std::memory_order_relaxed))
                    {
                        tail_chunk.store(new_chunk, std::memory_order_release);
                    }
                    else
                    {
                        // 其它线程已经添加了新块，释放我们创建的块
                        release_chunk(new_chunk);
                        if (expected)
                        {
                            tail_chunk.compare_exchange_strong(tail, expected, std::memory_order_release, std::memory_order_relaxed);
                        }
                    }
                }
            }
        }

        bool pop(T &out)
        {
            while (true)
            {
                Chunk *head = head_chunk.load(std::memory_order_acquire);
                if (!head)
                    return false;

                size_t index = head->pop_index.load(std::memory_order_acquire);
                size_t push_idx = head->push_index.load(std::memory_order_acquire);

                if (index >= head->size)
                {
                    // 当前块已经消费完，移动到下一个块
                    Chunk *next = head->next.load(std::memory_order_acquire);
                    if (next && head_chunk.compare_exchange_strong(head, next, std::memory_order_release, std::memory_order_relaxed))
                    {
                        retire_chunk(head);
                    }
                    else if (!next)
                    {
                        return false;
                    }
                    continue;
                }

                if (index >= push_idx)
                {
                    Chunk *next = head->next.load(std::memory_order_acquire);
                    if (!next)
                    {
                        return false;
                    }
                    if (head_chunk.compare_exchange_strong(head, next, std::memory_order_release, std::memory_order_relaxed))
                    {
                        retire_chunk(head);
                    }
                    continue;
                }

                if (!head->pop_index.compare_exchange_weak(index, index + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
                {
                    continue;
                }

                head->active_readers.fetch_add(1, std::memory_order_acquire);

                Node &node = head->nodes[index];
                while (!node.occupied.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }

                // 读取数据到 out
                out = std::move(*node.data_ptr());
                // 调用析构函数
                node.destroy();

                head->active_readers.fetch_sub(1, std::memory_order_release);
                try_release_chunk(head);

                return true;
            }
        }

    private:
        Chunk *allocate_chunk()
        {
            // 首先尝试从空闲列表获取
            Chunk *chunk = free_list.load(std::memory_order_acquire);
            while (chunk)
            {
                if (free_list.compare_exchange_weak(chunk, chunk->next.load(std::memory_order_relaxed), std::memory_order_release, std::memory_order_relaxed))
                {
                    // 重置块状态
                    chunk->push_index.store(0, std::memory_order_relaxed);
                    chunk->pop_index.store(0, std::memory_order_relaxed);
                    chunk->next.store(nullptr, std::memory_order_relaxed);
                    chunk->active_readers.store(0, std::memory_order_relaxed);
                    chunk->retired.store(false, std::memory_order_relaxed);
                    return chunk;
                }
            }

            // 没有可重用的块，创建新块
            chunk_count.fetch_add(1, std::memory_order_relaxed);
            return new Chunk(CHUNK_SIZE);
        }

        void release_chunk(Chunk *chunk)
        {
            if (!chunk)
                return;

            // 将块加入空闲列表
            Chunk *old_head = free_list.load(std::memory_order_acquire);
            chunk->next.store(old_head, std::memory_order_relaxed);

            while (!free_list.compare_exchange_weak(old_head, chunk, std::memory_order_release, std::memory_order_relaxed))
            {
                chunk->next.store(old_head, std::memory_order_relaxed);
            }
        }

        void retire_chunk(Chunk *chunk)
        {
            if (!chunk)
                return;
            chunk->retired.store(true, std::memory_order_release);
            try_release_chunk(chunk);
        }

        void try_release_chunk(Chunk *chunk)
        {
            if (!chunk)
                return;
            if (chunk->active_readers.load(std::memory_order_acquire) == 0)
            {
                bool expected = true;
                if (chunk->retired.compare_exchange_strong(expected, false, std::memory_order_acq_rel, std::memory_order_relaxed))
                {
                    release_chunk(chunk);
                }
            }
        }
    };
}