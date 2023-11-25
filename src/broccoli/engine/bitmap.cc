#include "broccoli/engine/bitmap.hh"

namespace broccoli {
  Bitmap::Bitmap(glm::i64vec3 size, void *pixels, bool owns_pixels)
  : m_pixels(pixels),
    m_size(size),
    m_owns_pixels(owns_pixels)
  {}
  Bitmap::Bitmap(glm::i64vec3 size)
  : Bitmap(size, calloc(size.x * size.y, size.z), true)
  {}
  Bitmap::Bitmap()
  : Bitmap(glm::i32vec3{0}, nullptr, false)
  {}
  Bitmap::Bitmap(Bitmap &&other)
  : Bitmap(other.m_size, other.m_pixels, other.m_owns_pixels)
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
}
namespace broccoli {
  Bitmap Bitmap::clone() const {
    Bitmap copy{m_size};
    memcpy(copy.data(), this->data(), m_size.x * m_size.y * m_size.z);
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
    m_size = glm::i64vec3{0};
    m_owns_pixels = false;
  }
}
namespace broccoli {
  void Bitmap::blit(Bitmap const &src) {
    blit(src, Rect{glm::i64vec2{0}, src.size()});
  }
  void Bitmap::blit(Bitmap const &src, Rect src_clip) {
    blit(src, src_clip, glm::i64vec3{0});
  }
  void Bitmap::blit(Bitmap const &src, Rect src_clip, glm::i64vec2 dst_offset) {
    CHECK(m_size.z == src.size().z, "Expected blit source and destination to have identical depths");
    auto depth = m_size.z;
    glm::i32vec2 src_area = glm::max(src_clip.max_xy - src_clip.min_xy, glm::i64vec2{0});
    glm::i32vec2 dst_area = glm::max(glm::i64vec2{m_size} - dst_offset, glm::i64vec2{0});
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
    return {glm::i64vec3{size_x, size_y, depth}, memory, true};
  }
  Bitmap Bitmap::loadFromMemory(glm::i64vec3 size, void *pixels, bool owns_pixels) {
    return {size, pixels, owns_pixels};
  }
}
