// simple_test.cpp - 简化测试
#include "scheduler/task.h"
#include <iostream>

Task<int> simple_test() {
    std::cout << "Simple test started" << std::endl;
    co_return 42;
}

int main() {
    auto task = simple_test();
    int result = task.get();
    std::cout << "Result: " << result << std::endl;
    return 0;
}