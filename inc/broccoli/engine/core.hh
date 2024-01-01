#pragma once

#include <functional>
#include <string>
#include <array>
#include <span>
#include <concepts>
#include <type_traits>
#include <unordered_map>

#define _USE_MATH_DEFINES
#include <cstdint>
#include <cmath>

#include <glm/glm.hpp>

#include "fmt/std.h"

//
// Panic, Check:
//

namespace broccoli {
  [[noreturn]] void panic(std::string message, const char *file, int64_t line);
  void check(bool condition, const char *codestr, const char *more, const char *file, int64_t line);
  void check(bool condition, const char *codestr, std::function<std::string()> more, const char *file, int64_t line);
}

#define PANIC(...)  	    broccoli::panic(fmt::format(__VA_ARGS__), __FILE__, __LINE__)
#define CHECK(COND, MORE) broccoli::check((COND), #COND, (MORE), __FILE__, __LINE__)

#ifdef NDEBUG
# define DEBUG_CHECK(COND, MORE) /* no expansion: disabled in release mode */
#else
# define DEBUG_CHECK(COND, MORE) CHECK((COND), MORE)
#endif

//
// File I/O:
//

namespace broccoli {
  std::string readTextFile(const char *file_path);
}

//
// StandardLayout (similar to POD)
//

namespace broccoli {
  template <typename T>
  concept StandardLayout = requires {
    std::is_standard_layout<T>::value;
  };
}

//
// IsZero, IsNonZero concepts:
//

namespace broccoli {
  template <uint32_t v> concept IsPositiveU32 = v > 0;
  template <uint32_t v> concept IsZeroU32 = v == 0;
}

//
// EnumType, __EnumArray, EnumMap
//

namespace broccoli {
  template <typename T>
  concept EnumType = std::is_enum_v<T>;
}
namespace broccoli {
  template <EnumType E>
  constexpr size_t enum_count() {
    return static_cast<size_t>(E::Metadata_Count);
  }
}
namespace broccoli {
  template <EnumType E, typename T>
  class EnumMap: public std::array<T, enum_count<E>()> {
  private:
    using Base = std::array<T, enum_count<E>()>;
  public:
    using Base::Base;
    using Base::data;
    using Base::size;
    using Base::operator[];
  public:
    T &operator[] (E key);
    constexpr T const &operator[] (E key) const;
  };
}
namespace broccoli {
  template <EnumType E, typename T>
  T &EnumMap<E, T>::operator[] (E key) {
    return Base::operator[](static_cast<size_t>(key));
  }
  template <EnumType E, typename T>
  constexpr T const &EnumMap<E, T>::operator[] (E key) const {
    return Base::operator[](static_cast<size_t>(key));
  }
}

//
// String replacement:
//

namespace broccoli {
  std::string replaceAll(const std::string &s, std::unordered_map<std::string, std::string> const &rw_map);
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

//
// Math types:
//

namespace broccoli {
  template <StandardLayout T>
  struct Rect {
    glm::tvec2<T> min_xy;
    glm::tvec2<T> max_xy;
  };
  using Rectf = Rect<float>;
}
