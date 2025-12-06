cmake_minimum_required(VERSION 3.17)
project(dRPC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build unit tests" ON)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)

set(DRPC_SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(DRPC_CORE_SOURCES
    net/accepter.cpp
    net/socket.cpp
    net/socket_utils.cpp
    net/connection.cpp
    scheduler/awaitable.cpp
    scheduler/epoll_executor.cpp
    scheduler/scheduler.cpp
    server/rpc_server.cpp
    client/client_channel.cpp
)

add_library(drpc_core STATIC ${DRPC_CORE_SOURCES})

target_include_directories(drpc_core
    PUBLIC
        ${DRPC_SRC_ROOT}
        ${DRPC_SRC_ROOT}/net
        ${DRPC_SRC_ROOT}/scheduler
        ${DRPC_SRC_ROOT}/server
        ${DRPC_SRC_ROOT}/client
        ${DRPC_SRC_ROOT}/util
)

target_link_libraries(drpc_core
    PUBLIC
        Threads::Threads
        ${Protobuf_LIBRARIES}
)

if (Protobuf_FOUND)
    target_include_directories(drpc_core PUBLIC ${Protobuf_INCLUDE_DIRS})
endif()

add_executable(test_rpc_server
    test/test_rpc_server.cc
)
target_link_libraries(test_rpc_server
    PRIVATE
        drpc_core
)

add_executable(test_rpc_client
    test/test_rpc_client.cc
)
target_link_libraries(test_rpc_client
    PRIVATE
        drpc_core
)

if (BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/util/CMakeLists.txt")
    add_subdirectory(util)
endif()

message(STATUS "dRPC: C++ standard = ${CMAKE_CXX_STANDARD}")
message(STATUS "dRPC: BUILD_TESTS = ${BUILD_TESTS}")
