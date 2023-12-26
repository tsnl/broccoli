#pragma once

#include <concepts>

#include "glm/vec2.hpp"
#include "glm/gtx/extended_min_max.hpp"

#include "core.hh"

namespace broccoli {
  template <typename T>
  concept SubpixelType =
    std::is_same_v<T, uint8_t> ||
    std::is_same_v<T, float>;
}

namespace broccoli {
  /// @brief GenericBitmap represents a raster image with Y rows, X columns, and Z planes.
  template <SubpixelType Subpixel>
  class GenericBitmap {
  public:
    struct Rect { glm::i32vec2 min_xy; glm::i32vec2 max_xy; };
  private:
    void *m_pixels;
    glm::i32vec3 m_dim;
    bool m_owns_pixels;
  private:
    GenericBitmap(glm::i32vec3 dim, void *pixels, bool owns_pixels);
  public:
    GenericBitmap(glm::i32vec3 dim);
    GenericBitmap();
    GenericBitmap(GenericBitmap<Subpixel> &&other);
    ~GenericBitmap();
  public:
    inline glm::i32vec3 dim() const;
  public:
    inline Subpixel *operator() (glm::i32vec2 coord);
    inline Subpixel const *operator() (glm::i32vec2 coord) const;
    inline Subpixel *operator() (int32_t x, int32_t y);
    inline Subpixel const *operator() (int32_t x, int32_t y) const;
  public:
    void *data();
    void const *data() const;
    int32_t dataSize() const;
    int32_t pitch() const;
    int32_t rows() const;
  public:
    GenericBitmap<Subpixel> clone() const;
  private:
    void dispose();
    void release();
  public:
    void clear();
    void clear(std::span<Subpixel> clear_pixel);
  public:
    void blit(GenericBitmap<Subpixel> const &src);
    void blit(GenericBitmap<Subpixel> const &src, Rect src_clip);
    void blit(GenericBitmap<Subpixel> const &src, glm::i32vec2 dst_offset);
    void blit(GenericBitmap<Subpixel> const &src, Rect src_clip, glm::i32vec2 dst_offset);
  public:
    GenericBitmap<Subpixel>& operator=(GenericBitmap<Subpixel> &&other);
  };
}

//
// Inline definitions:
//

namespace broccoli {
  template <SubpixelType Subpixel>
  inline glm::i32vec3 GenericBitmap<Subpixel>::dim() const {
    return m_dim;
  }
}
namespace broccoli {
  template <SubpixelType Subpixel>
  inline Subpixel *GenericBitmap<Subpixel>::operator() (glm::i32vec2 coord) {
    return this->operator()(coord.x, coord.y);
  }
  template <SubpixelType Subpixel>
  inline Subpixel const *GenericBitmap<Subpixel>::operator() (glm::i32vec2 coord) const {
    return this->operator()(coord.x, coord.y);
  }
  template <SubpixelType Subpixel>
  inline Subpixel *GenericBitmap<Subpixel>::operator() (int32_t x, int32_t y) {
    return &reinterpret_cast<Subpixel*>(m_pixels)[(y * m_dim.x * m_dim.z) + (x * m_dim.z) + 0];
  }
  template <SubpixelType Subpixel>
  inline Subpixel const *GenericBitmap<Subpixel>::operator() (int32_t x, int32_t y) const {
    return &reinterpret_cast<Subpixel*>(m_pixels)[(y * m_dim.x * m_dim.z) + (x * m_dim.z) + 0];
  }
}

//
// Convenient exposed classes:
//

namespace broccoli {
  class Bitmap: public GenericBitmap<uint8_t> {
  public:
    using GenericBitmap<uint8_t>::GenericBitmap;
  public:
    static Bitmap loadFromFile(const char *filepath, int expected_depth);
    static Bitmap loadFromMemory(glm::i32vec3 dim, void *pixels, bool owns_pixels);
  public:
    void save(const char *filepath) const;
  };
  class FloatBitmap: public GenericBitmap<float> {
  public:
    using GenericBitmap<float>::GenericBitmap;
  public:
    Bitmap quantize(float minimum = 0.0f, float maximum = 1.0f);
    void save(const char *filepath) const;
  };
}
