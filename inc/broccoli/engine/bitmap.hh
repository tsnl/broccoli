#pragma once

#include "glm/vec2.hpp"
#include "glm/gtx/extended_min_max.hpp"
#include "stb/image.hh"

#include "core.hh"

namespace broccoli {
  /// @brief Bitmap represents a raster image with Y rows, X columns, and Z planes, such that we use 1B per subpixel.
  class Bitmap {
  public:
    struct Rect { glm::i64vec2 min_xy; glm::i64vec2 max_xy; };
  private:
    void *m_pixels;
    glm::i64vec3 m_size;
    bool m_owns_pixels;
  private:
    Bitmap(glm::i64vec3 size, void *pixels, bool owns_pixels);
  public:
    Bitmap(glm::i64vec3 size);
    Bitmap();
    Bitmap(Bitmap &&other);
    ~Bitmap();
  public:
    inline glm::i64vec3 size() const;
  public:
    inline uint8_t *operator() (glm::i64vec2 coord);
    inline uint8_t const *operator() (glm::i64vec2 coord) const;
    inline uint8_t *operator() (int64_t x, int64_t y);
    inline uint8_t const *operator() (int64_t x, int64_t y) const;
  public:
    void *data();
    void const *data() const;
    size_t dataSize() const;
    size_t pitch() const;
    size_t rows() const;
  public:
    Bitmap clone() const;
  private:
    void dispose();
    void release();
  public:
    void blit(Bitmap const &src);
    void blit(Bitmap const &src, Rect src_clip);
    void blit(Bitmap const &src, Rect src_clip, glm::i64vec2 dst_offset);
  public:
    static Bitmap loadFromFile(const char *filepath, int expected_depth);
    static Bitmap loadFromMemory(glm::i64vec3 size, void *pixels, bool owns_pixels);
  };
}

//
// Inline definitions:
//

namespace broccoli {
  inline glm::i64vec3 Bitmap::size() const {
    return m_size;
  }
}
namespace broccoli {
  inline uint8_t *Bitmap::operator() (glm::i64vec2 coord) {
    return this->operator()(coord.x, coord.y);
  }
  inline uint8_t const *Bitmap::operator() (glm::i64vec2 coord) const {
    return this->operator()(coord.x, coord.y);
  }
  inline uint8_t *Bitmap::operator() (int64_t x, int64_t y) {
    return &reinterpret_cast<uint8_t*>(m_pixels)[(y * m_size.x * m_size.z) + (x * m_size.z) + 0];
  }
  inline uint8_t const *Bitmap::operator() (int64_t x, int64_t y) const {
    return &reinterpret_cast<uint8_t*>(m_pixels)[(y * m_size.x * m_size.z) + (x * m_size.z) + 0];
  }
}
