#pragma once

#include <functional>
#include <string>
#include <array>
#include <span>
#include <concepts>
#include <type_traits>
#include <cstdint>

#include "fmt/std.h"

//
// Panic, Check:
//

namespace broccoli {
  void panic(std::string message, const char *file, int64_t line);
  void check(bool condition, const char *codestr, const char *more, const char *file, int64_t line);
  void check(bool condition, const char *codestr, std::function<std::string()> more, const char *file, int64_t line);
}

#define PANIC(...)  	    broccoli::panic(fmt::format(__VA_ARGS__), __FILE__, __LINE__)
#define CHECK(COND, MORE) broccoli::check((COND), #COND, (MORE), __FILE__, __LINE__)

//
// File I/O:
//

namespace broccoli {
  std::string readTextFile(const char *file_path);
}

//
// Hashing:
//

namespace broccoli {
  template <typename T>
  concept HasherBackend = requires(T a, uint8_t b) {
    a.write(b);
    { a.finish() } -> std::same_as<uint64_t>;
  };
  template <typename T>
  concept StandardLayout = requires {
    std::is_standard_layout<T>::value;
  };
}
namespace broccoli {
  template <HasherBackend Backend>
  class Hasher {
  private:
    Backend m_backend;
  public:
    inline void write(uint8_t byte);
    inline void write(std::span<const uint8_t> bytes);
    template <size_t extent> inline void write(std::span<const uint8_t, extent> bytes);
    template <StandardLayout Pod> inline void write(Pod const *pod);
    inline uint64_t finish() const;
  };
  template <HasherBackend Backend>
  inline void Hasher<Backend>::write(uint8_t byte) {
    m_backend.write(byte);
  }
  template <HasherBackend Backend>
  inline void Hasher<Backend>::write(std::span<const uint8_t> bytes) {
    for (auto b: bytes) {
      write(b);
    }
  }
  template <HasherBackend Backend>
  template <size_t extent>
  inline void Hasher<Backend>::write(std::span<const uint8_t, extent> bytes) {
    for (auto b: bytes) {
      write(b);
    }
  }
  template <HasherBackend Backend>
  template <StandardLayout Pod>
  inline void Hasher<Backend>::write(Pod const *pod) {
    using Span = std::span<const uint8_t, sizeof(Pod)>;
    auto ptr = reinterpret_cast<const std::array<const uint8_t, sizeof(Pod)>*>(pod);
    write(Span{*ptr});
  }
  template <HasherBackend Backend>
  inline uint64_t Hasher<Backend>::finish() const {
    return m_backend.finish();
  }
}

namespace broccoli {
  class Fnv1aHasherBackend {
  private:
    uint64_t m_hash;
  public:
    inline Fnv1aHasherBackend();
  public:
    inline void write(uint8_t byte);
    inline uint64_t finish() const;
  };
  inline Fnv1aHasherBackend::Fnv1aHasherBackend()
  : m_hash(0XCBF29CE484222325)
  {}
  inline void Fnv1aHasherBackend::write(uint8_t byte) {
    m_hash ^= byte;
    m_hash *= 0x00000100000001B3;
  }
  inline uint64_t Fnv1aHasherBackend::finish() const {
    return m_hash;
  }
  using Fnv1aHasher = Hasher<Fnv1aHasherBackend>;
}
