cmake_minimum_required(VERSION 3.14)
project(MPMCQueueTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 GTest
find_package(GTest REQUIRED)

# 添加可执行文件
add_executable(mpmc_queue_test 
    mpmc_queue_test.cpp
)

# 链接 GTest
target_link_libraries(mpmc_queue_test GTest::GTest GTest::Main pthread)

# 启用测试
enable_testing()
add_test(NAME MPMCQueueTest COMMAND mpmc_queue_test)