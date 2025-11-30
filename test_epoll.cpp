#include "scheduler/task.h"
#include "scheduler/epoll_executor.h"
#include "scheduler/awaitable.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <unistd.h>

Task<void> test_epoll_operations(EpollExecutor &executor)
{
    // 创建管道测试
    int pipefd[2];
    if (pipe(pipefd) < 0)
    {
        throw std::runtime_error("pipe failed");
    }

    std::cout << "Testing write await..." << std::endl;
    co_await wait_writable(pipefd[1], executor);
    std::cout << "Write await completed" << std::endl;

    std::cout << "Testing read await..." << std::endl;

    // 在后台线程写入数据
    std::thread writer([pipefd]
                       {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        write(pipefd[1], "test", 4);
        std::cout << "Data written to pipe" << std::endl; });

    co_await wait_readable(pipefd[0], executor);
    std::cout << "Read await completed" << std::endl;

    // 读取数据
    char buffer[10];
    ssize_t n = read(pipefd[0], buffer, sizeof(buffer));
    std::cout << "Read " << n << " bytes: " << std::string(buffer, n) << std::endl;

    writer.join();
    close(pipefd[0]);
    close(pipefd[1]);
}

int main()
{
    try
    {
        EpollExecutor executor;

        auto task = test_epoll_operations(executor);

        // 启动协程，使其向 epoll 注册等待者
        if (auto h = task.get_handle())

        {
            h.resume();
        }

        std::cout << "Starting event loop..." << std::endl;
        executor.Run();

        std::cout << "Event loop finished, getting task result..." << std::endl;
        task.get();

        std::cout << "All tests passed!" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}