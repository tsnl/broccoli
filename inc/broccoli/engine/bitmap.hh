#pragma once

#include "glm/vec2.hpp"
#include "glm/gtx/extended_min_max.hpp"
#include "stb/image.hh"

#include "core.hh"

namespace broccoli {
  /// @brief Bitmap represents a raster image with Y rows, X columns, and Z planes, such that we use 1B per subpixel.
  class Bitmap {
  public:
    struct Rect { glm::i32vec2 min_xy; glm::i32vec2 max_xy; };
  private:
    void *m_pixels;
    glm::i32vec3 m_dim;
    bool m_owns_pixels;
  private:
    Bitmap(glm::i32vec3 dim, void *pixels, bool owns_pixels);
  public:
    Bitmap(glm::i32vec3 dim);
    Bitmap();
    Bitmap(Bitmap &&other);
    ~Bitmap();
  public:
    inline glm::i32vec3 dim() const;
  public:
    inline uint8_t *operator() (glm::i32vec2 coord);
    inline uint8_t const *operator() (glm::i32vec2 coord) const;
    inline uint8_t *operator() (int32_t x, int32_t y);
    inline uint8_t const *operator() (int32_t x, int32_t y) const;
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
    void blit(Bitmap const &src, Rect src_clip, glm::i32vec2 dst_offset);
  public:
    static Bitmap loadFromFile(const char *filepath, int expected_depth);
    static Bitmap loadFromMemory(glm::i32vec3 dim, void *pixels, bool owns_pixels);
  };
}

//
// Inline definitions:
//

namespace broccoli {
  inline glm::i32vec3 Bitmap::dim() const {
    return m_dim;
  }
}
namespace broccoli {
  inline uint8_t *Bitmap::operator() (glm::i32vec2 coord) {
    return this->operator()(coord.x, coord.y);
  }
  inline uint8_t const *Bitmap::operator() (glm::i32vec2 coord) const {
    return this->operator()(coord.x, coord.y);
  }
  inline uint8_t *Bitmap::operator() (int32_t x, int32_t y) {
    return &reinterpret_cast<uint8_t*>(m_pixels)[(y * m_dim.x * m_dim.z) + (x * m_dim.z) + 0];
  }
  inline uint8_t const *Bitmap::operator() (int32_t x, int32_t y) const {
    return &reinterpret_cast<uint8_t*>(m_pixels)[(y * m_dim.x * m_dim.z) + (x * m_dim.z) + 0];
  }
}
