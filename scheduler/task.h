#pragma once

#include <coroutine>
#include <exception>
#include <type_traits>
#include <utility>
#include <stdexcept>

template <typename T>
class Task;

namespace detail
{
    // 协程承诺分配器
    struct TaskAllocator
    {
        static void *operator new(size_t size)
        {
            return ::operator new(size);
        }

        static void operator delete(void *ptr, size_t size)
        {
            ::operator delete(ptr);
        }
    };

    // 非void类型
    template <typename T>
    struct TaskPromise : TaskAllocator
    {
        std::coroutine_handle<> continuation_; // 续体协程
        std::exception_ptr exception_;         // 存储的异常
        T value_;

        TaskPromise() = default;

        // 创建Task对象
        Task<T> get_return_object() noexcept;

        // 初始挂起 - 总是挂起，让调用者控制执行
        std::suspend_always initial_suspend() noexcept
        {
            return {};
        }

        // 最终挂起 - 转移到续体协程
        auto final_suspend() noexcept
        {
            struct FinalAwaiter
            {
                bool await_ready() noexcept { return false; }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<TaskPromise> h) noexcept
                {
                    auto &promise = h.promise();
                    return promise.continuation_ ? promise.continuation_ : std::noop_coroutine();
                }

                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }

        void unhandled_exception() noexcept
        {
            exception_ = std::current_exception();
        }

        // 设置续体
        void set_continuation(std::coroutine_handle<> continuation) noexcept
        {
            continuation_ = continuation;
        }

        template <typename U>
            requires std::convertible_to<U &&, T>
        void return_value(U &&value)
        {
            value_ = std::forward<U>(value);
        }

        T &result() &
        {
            if (exception_)
            {
                std::rethrow_exception(exception_);
            }
            return value_;
        }

        T &&result() &&
        {
            if (exception_)
            {
                std::rethrow_exception(exception_);
            }
            return std::move(value_);
        }
    };

    // void特化
    template <>
    struct TaskPromise<void> : TaskAllocator
    {
        std::coroutine_handle<> continuation_;
        std::exception_ptr exception_;

        TaskPromise() = default;

        Task<void> get_return_object() noexcept;

        std::suspend_always initial_suspend() noexcept
        {
            return {};
        }

        auto final_suspend() noexcept
        {
            struct FinalAwaiter
            {
                bool await_ready() noexcept { return false; }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<TaskPromise> h) noexcept
                {
                    auto &promise = h.promise();
                    return promise.continuation_ ? promise.continuation_ : std::noop_coroutine();
                }

                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }

        void unhandled_exception() noexcept
        {
            exception_ = std::current_exception();
        }

        void set_continuation(std::coroutine_handle<> continuation) noexcept
        {
            continuation_ = continuation;
        }

        void return_void() noexcept {}

        void result()
        {
            if (exception_)
            {
                std::rethrow_exception(exception_);
            }
        }
    };
}

template <typename T>
class Task
{
public:
    using promise_type = detail::TaskPromise<T>;

    Task() noexcept = default;

    explicit Task(std::coroutine_handle<promise_type> coro) noexcept : coro_(coro) {}

    Task(Task &&other) noexcept : coro_(std::exchange(other.coro_, {})) {}

    Task &operator=(Task &&other) noexcept
    {
        if (this != &other)
        {
            if (coro_)
            {
                coro_.destroy();
            }
            coro_ = std::exchange(other.coro_, {});
        }
        return *this;
    }

    ~Task()
    {
        if (coro_)
        {
            coro_.destroy();
        }
    }

    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;

    bool valid() const noexcept
    {
        return static_cast<bool>(coro_);
    }

    bool is_ready() const noexcept
    {
        return coro_ && coro_.done();
    }

    T get()
    {
        if (!coro_)
            throw std::runtime_error("invalid task");

        // 驱动协程直到完成
        while (!coro_.done())
            coro_.resume();

        auto &promise = coro_.promise();

        // 如果有异常，先保存再销毁协程句柄，最后重新抛出
        if (promise.exception_)
        {
            auto e = promise.exception_;
            coro_.destroy();
            coro_ = nullptr;
            std::rethrow_exception(e);
        }

        if constexpr (!std::is_void_v<T>)
        {
            T value = std::move(promise.value_);
            coro_.destroy();
            coro_ = nullptr;
            return value;
        }
        else
        {
            // void 特化：没有返回值，仅销毁协程句柄
            coro_.destroy();
            coro_ = nullptr;
        }
    }

    std::coroutine_handle<promise_type> get_handle() noexcept
    {
        return coro_;
    }

    void set_continuation(std::coroutine_handle<> continuation) noexcept
    {
        if (coro_)
        {
            coro_.promise().set_continuation(continuation);
        }
    }

    auto operator co_await() const & noexcept
    {
        struct TaskAwaiter
        {
            std::coroutine_handle<promise_type> coro_;

            TaskAwaiter(std::coroutine_handle<promise_type> coro) : coro_(coro) {}

            bool await_ready() const noexcept
            {
                return !coro_ || coro_.done();
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_coro) noexcept
            {
                if (coro_ && !coro_.done())
                {
                    coro_.promise().set_continuation(awaiting_coro);
                    return coro_;
                }
                return awaiting_coro;
            }

            T await_resume()
            {
                if (!coro_)
                {
                    throw std::runtime_error("Awaiting empty Task");
                }
                return coro_.promise().result();
            }
        };

        return TaskAwaiter{coro_};
    }

    auto operator co_await() && noexcept
    {
        struct TaskAwaiter
        {
            std::coroutine_handle<promise_type> coro_;

            TaskAwaiter(std::coroutine_handle<promise_type> coro) : coro_(coro) {}

            bool await_ready() const noexcept
            {
                return !coro_ || coro_.done();
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_coro) noexcept
            {
                if (coro_ && !coro_.done())
                {
                    coro_.promise().set_continuation(awaiting_coro);
                    return coro_;
                }
                return awaiting_coro;
            }

            T await_resume()
            {
                if (!coro_)
                {
                    throw std::runtime_error("Awaiting empty Task");
                }
                return std::move(coro_.promise()).result();
            }
        };

        return TaskAwaiter{std::exchange(coro_, {})};
    }

private:
    std::coroutine_handle<promise_type> coro_ = nullptr;
};

template <typename T>
inline Task<T> detail::TaskPromise<T>::get_return_object() noexcept
{
    return Task<T>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
}

inline Task<void> detail::TaskPromise<void>::get_return_object() noexcept
{
    return Task<void>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
}

using VoidTask = Task<void>;

template <typename T>
Task<T> make_ready_task(T value)
{
    co_return value;
}

inline Task<void> make_ready_task()
{
    co_return;
}

template <typename T>
Task<T> make_exception_task(std::exception_ptr e)
{
    std::rethrow_exception(e);
    if constexpr (!std::is_void_v<T>)
    {
        co_return T{};
    }
    else
    {
        co_return;
    }
}