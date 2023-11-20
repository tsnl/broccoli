#include "broccoli/core.hh"

#include <iostream>
#include <format>

#include <cstdlib>

namespace broccoli {
  void panic(std::string message, const char *file, int64_t line) {
    std::cerr 
      << "ERROR: " << message << std::endl
      << "- see: " << file << ':' << line << std::endl;
    std::exit(1);
  }
  void check(bool condition, const char *codestr, const char *more, const char *file, int64_t line) {
    if (!condition) [[unlikely]] {
      panic(fmt::format("check failed: {}\n- condition: {}", more, codestr), file, line);
    }
  }
  void check(bool condition, const char *codestr, std::function<std::string()> more, const char *file, int64_t line) {
    if (!condition) [[unlikely]] {
      panic(fmt::format("check failed: {}\n- condition: {}", more(), codestr), file, line);
    }
  }
}
