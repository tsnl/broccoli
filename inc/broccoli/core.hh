#pragma once

#include <string>
#include <functional>

#include <cstdint>

#include "fmt/std.h"

namespace broccoli {
  void panic(std::string message, const char *file, int64_t line);
  void check(bool condition, const char *codestr, const char *more, const char *file, int64_t line);
  void check(bool condition, const char *codestr, std::function<std::string()> more, const char *file, int64_t line);
}

#define PANIC(...)  	    broccoli::panic(fmt::format(__VA_ARGS__), __FILE__, __LINE__)
#define CHECK(COND, MORE) broccoli::check((COND), #COND, (MORE), __FILE__, __LINE__)
