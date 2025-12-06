#include <iostream>
#include <format>
#include <functional>

using Closure = std::function<void()>;

#define info(fmt, ...) std::cout << std::format(fmt, ##__VA_ARGS__) << std::endl;
#define error(fmt, ...) std::cerr << std::format(fmt, ##__VA_ARGS__) << std::endl;