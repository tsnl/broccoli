#include "broccoli/engine/core.hh"

#include <iostream>
#include <fstream>
#include <format>
#include <sstream>

#include <cstdlib>
#include <csignal>

//
// Panic, Check:
//

namespace broccoli {
  void panic(std::string message, const char *file, int64_t line) {
    std::cerr 
      << "ERROR: " << message << std::endl
      << "- see: " << file << ':' << line << std::endl;
    std::abort();
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
    res.resize(file_size);
    f.seekg(0, std::ios_base::beg);
    f.read(res.data(), file_size);
    return res;
  }
}

//
// String replacement:
//

namespace broccoli {
  static std::string replaceAllHelper(const std::string &s, const std::string &find, const std::string &replacement) {
    size_t offset = 0;
    std::stringstream res;
    while (offset < s.size()) {
      size_t found = s.find(find, offset);
      for (auto copy_idx = offset; copy_idx < std::min(s.size(), found); copy_idx++) {
        res << s[copy_idx];
      }
      if (found == std::string::npos) {
        break;
      }
      res << replacement;
      offset = found + find.size();
    }
    return res.str();
  }
  std::string replaceAll(const std::string &s, std::unordered_map<std::string, std::string> const &rw_map) {
    // TODO: switch to a more efficient Regex-based rewriting solution.
    
    // First, sort all keys by length:
    std::vector<std::string> replaced_vec;
    replaced_vec.reserve(rw_map.size());
    for (const auto &[replaced, _]: rw_map) {
      replaced_vec.push_back(replaced);
    }
    std::sort(
      replaced_vec.begin(), replaced_vec.end(), 
      [] (const std::string &lt, const std::string &rt) -> bool {
        return lt.size() > rt.size();
      }
    );

    // Next, replace from longest key to shortest:
    std::string acc = s;
    for (const auto &replaced: replaced_vec) {
      acc = replaceAllHelper(std::move(acc), replaced, rw_map.find(replaced)->second);
    }
    
    return acc;
  }
}
