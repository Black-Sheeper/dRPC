#pragma once

#include <iostream>
#include <format>
#include <functional>

using Closure = std::function<void()>;

constexpr inline uint64_t MAGIC_NUM = 0x30F8CA9B;
constexpr inline int32_t VERSION = 1;

#define info(fmt, ...) std::cout << std::format(fmt, ##__VA_ARGS__) << std::endl;
#define error(fmt, ...) std::cerr << std::format(fmt, ##__VA_ARGS__) << std::endl;