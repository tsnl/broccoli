#include "broccoli/engine/bitmap.hh"
#include "stb/image.hh"
#include "stb/image_write.hh"

//
// GenericBitmap:
//

namespace broccoli {
  template <SubpixelType Subpixel>
  GenericBitmap<Subpixel>::GenericBitmap(glm::i32vec3 size, void *pixels, bool owns_pixels)
  : m_pixels(pixels),
    m_dim(size),
    m_owns_pixels(owns_pixels)
  {}
  template <SubpixelType Subpixel>
  GenericBitmap<Subpixel>::GenericBitmap(glm::i32vec3 size)
  : GenericBitmap(
      size, 
      size.x * size.y * size.z == 0 ? nullptr : calloc(size.x * size.y, size.z * sizeof(Subpixel)), 
      true
    )
  {}
  template <SubpixelType Subpixel>
  GenericBitmap<Subpixel>::GenericBitmap()
  : GenericBitmap(glm::i32vec3{0}, nullptr, false)
  {}
  template <SubpixelType Subpixel>
  GenericBitmap<Subpixel>::GenericBitmap(GenericBitmap &&other)
  : GenericBitmap(other.m_dim, other.m_pixels, other.m_owns_pixels)
  {
    other.release();
  }
  template <SubpixelType Subpixel>
  GenericBitmap<Subpixel>::~GenericBitmap() {
    dispose();
  }
}
namespace broccoli {
  template <SubpixelType Subpixel>
  void *GenericBitmap<Subpixel>::data() {
    return m_pixels;
  }
  template <SubpixelType Subpixel>
  void const *GenericBitmap<Subpixel>::data() const {
    return m_pixels;
  }
  template <SubpixelType Subpixel>
  int32_t GenericBitmap<Subpixel>::dataSize() const {
    return pitch() * rows();
  }
  template <SubpixelType Subpixel>
  int32_t GenericBitmap<Subpixel>::pitch() const {
    return m_dim.x * m_dim.z * static_cast<int32_t>(sizeof(Subpixel));
  }
  template <SubpixelType Subpixel>
  int32_t GenericBitmap<Subpixel>::rows() const {
    return static_cast<size_t>(m_dim.y);
  }
}
namespace broccoli {
  template <SubpixelType Subpixel>
  GenericBitmap<Subpixel> GenericBitmap<Subpixel>::clone() const {
    GenericBitmap copy{m_dim};
    memcpy(copy.data(), this->data(), m_dim.x * m_dim.y * m_dim.z);
    return copy;
  }
}
namespace broccoli {
  template <SubpixelType Subpixel>
  void GenericBitmap<Subpixel>::dispose() {
    if (m_pixels && m_owns_pixels) {
      free(m_pixels);
    }
    release();
  }
  template <SubpixelType Subpixel>
  void GenericBitmap<Subpixel>::release() {
    m_pixels = nullptr;
    m_dim = glm::i32vec3{0};
    m_owns_pixels = false;
  }
}
namespace broccoli {
  template <SubpixelType Subpixel>
  void GenericBitmap<Subpixel>::clear() {
    memset(data(), 0, dataSize());
  }
  template <SubpixelType Subpixel>
  void GenericBitmap<Subpixel>::clear(std::span<Subpixel> clear_pixel) {
    CHECK(clear_pixel.size() == m_dim.z, "Invalid clear pixel value: depth incorrect.");
    for (int32_t y = 0; y < m_dim.y; y++) {
      for (int32_t x = 0; x < m_dim.x; x++) {
        memcpy(this->operator()(x, y), clear_pixel.data(), clear_pixel.size_bytes());
      }
    }
  }
}
namespace broccoli {
  template <SubpixelType Subpixel>
  void GenericBitmap<Subpixel>::blit(GenericBitmap const &src) {
    blit(src, Rect{glm::i32vec2{0}, src.dim()});
  }
  template <SubpixelType Subpixel>
  void GenericBitmap<Subpixel>::blit(GenericBitmap const &src, Rect src_clip) {
    blit(src, src_clip, glm::i32vec3{0});
  }
  template <SubpixelType Subpixel>
  void GenericBitmap<Subpixel>::blit(GenericBitmap const &src, glm::i32vec2 dst_offset) {
    blit(src, Rect{glm::i32vec2{0}, src.dim()}, dst_offset);
  }
  template <SubpixelType Subpixel>
  void GenericBitmap<Subpixel>::blit(GenericBitmap const &src, Rect src_clip, glm::i32vec2 dst_offset) {
    CHECK(m_dim.z == src.dim().z, "Expected blit source and destination to have identical depths");
    auto depth = m_dim.z;
    glm::i32vec2 src_area = glm::max(src_clip.max_xy - src_clip.min_xy, glm::i32vec2{0});
    glm::i32vec2 dst_area = glm::max(glm::i32vec2{m_dim} - dst_offset, glm::i32vec2{0});
    glm::i32vec2 copy_area = glm::min(src_area, dst_area);
    if (copy_area.x == 0 || copy_area.y == 0) {
      return;
    }
    auto src_offset = src_clip.min_xy;
    for (int y_offset = 0; y_offset < copy_area.y; y_offset++) {
      void *p_dst_line = this->operator()(dst_offset.x, dst_offset.y + y_offset);
      void const *p_src_line = src.operator()(src_offset.x, src_offset.y + y_offset);
      memcpy(p_dst_line, p_src_line, static_cast<size_t>(copy_area.x) * depth * sizeof(Subpixel));
    }
  }
}
namespace broccoli {
  template <SubpixelType Subpixel>
  GenericBitmap<Subpixel>& GenericBitmap<Subpixel>::operator=(GenericBitmap<Subpixel> &&other) {
    this->dispose();
    new(this) GenericBitmap<Subpixel>(std::move(other));
    other.release();
    return *this;
  }
}

//
// Explicit template instantiations for linkage:
//

namespace broccoli {
  template class GenericBitmap<uint8_t>;
  template class GenericBitmap<float>;
}

//
// Bitmap-specific methods:
//

namespace broccoli {
  Bitmap Bitmap::loadFromFile(const char *filepath, int expected_depth) {
    int size_x, size_y, depth;
    stbi_uc *memory = stbi_load(filepath, &size_x, &size_y, &depth, expected_depth);
    CHECK(depth == expected_depth, "Expected channel count did not match file channel count");
    return {glm::i32vec3{size_x, size_y, depth}, memory, true};
  }
  Bitmap Bitmap::loadFromMemory(glm::i32vec3 size, void *pixels, bool owns_pixels) {
    return {size, pixels, owns_pixels};
  }
}
namespace broccoli {
  void Bitmap::save(const char *filepath) const {
    CHECK(0 == strcmp(strrchr(filepath, '.'), ".png"), "Expected bitmap save filepath to end with '.png' extension.");
    int ok = stbi_write_png(filepath, dim().x, dim().y, dim().z, data(), static_cast<int>(pitch()));
    CHECK(ok, "Something went wrong when saving a bitmap.");
  }
}

//
// FloatBitmap-specific methods:
//

namespace broccoli {
  Bitmap FloatBitmap::quantize(float minimum, float maximum) {
    auto d = dim();
    Bitmap res{d};
    for (int32_t y = 0; y < d.y; y++) {
      for (int32_t x = 0; x < d.x; x++) {
        for (int32_t z = 0; z < d.z; z++) {
          res(x, y)[z] = static_cast<uint8_t>(255.0f * ((this->operator()(x, y)[z] - minimum) / (maximum - minimum)));
        }
      }
    }
    return res;
  }
  void FloatBitmap::save(const char *filepath) const {
    CHECK(0 == strcmp(strrchr(filepath, '.'), ".hdr"), "Expected bitmap save filepath to end with '.hdr' extension.");
    int ok = stbi_write_hdr(filepath, dim().x, dim().y, dim().z, reinterpret_cast<const float *>(data()));
    CHECK(ok, "Something went wrong when saving a bitmap.");
  }
}
