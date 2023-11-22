#include "broccoli/engine/core.hh"

#include <iostream>
#include <fstream>
#include <format>

#include <cstdlib>

//
// Panic, Check:
//

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

//
// File I/O:
//

namespace broccoli {
  std::string readTextFile(const char *file_path) {
    std::ifstream f{file_path};
    CHECK(
      f.good(), 
      [file_path] () { return fmt::format("Failed to open text file:\nfilepath: {}", file_path); }
    );
    std::string res;
    f.seekg(0, std::ios_base::end);
    auto file_size = f.tellg();
    res.resize(1 + file_size);
    f.seekg(0, std::ios_base::beg);
    f.read(res.data(), file_size);
    res[file_size] = '\0';
    return res;
  }
}
