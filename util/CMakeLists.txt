cmake_minimum_required(VERSION 3.17)

# This CMakeLists is a subdirectory included by the top-level project.
# Configure test executable for the util unit tests.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(GTest REQUIRED)

add_executable(mpmc_queue_test
    mpmc_queue_test.cpp
)

target_include_directories(mpmc_queue_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(mpmc_queue_test PRIVATE cxx_std_20)

target_link_libraries(mpmc_queue_test
    PRIVATE
        GTest::GTest
        GTest::Main
        Threads::Threads
)

enable_testing()
add_test(NAME MPMCQueueTest COMMAND mpmc_queue_test)