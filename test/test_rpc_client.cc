#include "socket.h"
#include <iostream>
#include <string>
#include <thread>

void TestRpcClient()
{
    try
    {
        net::Socket socket;

        std::cout << "Connecting to RPC server..." << std::endl;
        if (socket.Connect("127.0.0.1", 8080))
        {
            std::cout << "Connected successfully!" << std::endl;

            // 发送测试消息
            std::string message = "Hello RPC Server!";
            std::cout << "Sending: " << message << std::endl;

            socket.Write(message.data(), message.size());

            // 接收响应
            char buffer[1024];
            int n = socket.Read(buffer, sizeof(buffer));
            if (n > 0)
            {
                std::string response(buffer, n);
                std::cout << "Received: " << response << std::endl;
            }
            else
            {
                std::cout << "No response" << std::endl;
            }
        }
        else
        {
            std::cout << "Failed to connect to server" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
}

int main()
{
    std::cout << "=== Testing RPC Client ===" << std::endl;

    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::seconds(1));

    TestRpcClient();

    return 0;
}