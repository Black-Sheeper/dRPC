#include "rpc_server.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    try {
        std::cout << "=== Starting RPC Server Test ===" << std::endl;
        
        // 创建RPC服务器
        server::RpcServer server(8080);
        
        // 注册测试服务方法
        server.RegisterService("TestService", "Echo", 
            [](const std::vector<char>& request) -> std::vector<char> {
                std::string message(request.begin(), request.end());
                std::string response = "Echo: " + message;
                return std::vector<char>(response.begin(), response.end());
            });
            
        server.RegisterService("TestService", "Add", 
            [](const std::vector<char>& request) -> std::vector<char> {
                // 简单的加法服务示例
                std::string response = "Add service result: 42";
                return std::vector<char>(response.begin(), response.end());
            });
        
        // 启动服务器
        server.Start();
        
        std::cout << "RPC Server is running on port 8080" << std::endl;
        std::cout << "Test with: telnet localhost 8080" << std::endl;
        std::cout << "Press Enter to stop..." << std::endl;
        
        // 等待用户输入
        std::cin.get();
        
        server.Stop();
        std::cout << "=== Test Completed ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}