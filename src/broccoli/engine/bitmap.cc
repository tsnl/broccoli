#include "broccoli/engine/bitmap.hh"

namespace broccoli {
  Bitmap::Bitmap(glm::i32vec3 size, void *pixels, bool owns_pixels)
  : m_pixels(pixels),
    m_dim(size),
    m_owns_pixels(owns_pixels)
  {
    CHECK(m_dim.x > 0, "Invalid 'x' dimensions for bitmap.");
    CHECK(m_dim.y > 0, "Invalid 'y' dimensions for bitmap.");
    CHECK(m_dim.z > 0, "Invalid 'z' dimensions for bitmap.");
  }
  Bitmap::Bitmap(glm::i32vec3 size)
  : Bitmap(size, calloc(size.x * size.y, size.z), true)
  {}
  Bitmap::Bitmap()
  : Bitmap(glm::i32vec3{0}, nullptr, false)
  {}
  Bitmap::Bitmap(Bitmap &&other)
  : Bitmap(other.m_dim, other.m_pixels, other.m_owns_pixels)
  {
    other.release();
  }
  Bitmap::~Bitmap() {
    dispose();
  }
}
namespace broccoli {
  void *Bitmap::data() {
    return m_pixels;
  }
  void const *Bitmap::data() const {
    return m_pixels;
  }
  size_t Bitmap::dataSize() const {
    return pitch() * rows();
  }
  size_t Bitmap::pitch() const {
    return 
      static_cast<size_t>(m_dim.x) * 
      static_cast<size_t>(m_dim.z);
  }
  size_t Bitmap::rows() const {
    return static_cast<size_t>(m_dim.y);
  }
}
namespace broccoli {
  Bitmap Bitmap::clone() const {
    Bitmap copy{m_dim};
    memcpy(copy.data(), this->data(), m_dim.x * m_dim.y * m_dim.z);
    return copy;
  }
}
namespace broccoli {
  void Bitmap::dispose() {
    if (m_pixels && m_owns_pixels) {
      free(m_pixels);
    }
    release();
  }
  void Bitmap::release() {
    m_pixels = nullptr;
    m_dim = glm::i32vec3{0};
    m_owns_pixels = false;
  }
}
namespace broccoli {
  void Bitmap::blit(Bitmap const &src) {
    blit(src, Rect{glm::i32vec2{0}, src.dim()});
  }
  void Bitmap::blit(Bitmap const &src, Rect src_clip) {
    blit(src, src_clip, glm::i32vec3{0});
  }
  void Bitmap::blit(Bitmap const &src, Rect src_clip, glm::i32vec2 dst_offset) {
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
      void *p_dst_line = this->operator()(dst_offset.x, dst_offset.y);
      void const *p_src_line = src.operator()(src_offset.x, src_offset.y);
      memcpy(p_dst_line, p_src_line, copy_area.x * depth);
    }
  }
}
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
